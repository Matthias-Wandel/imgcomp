//----------------------------------------------------------------------------------------
// Code to launch raspistill as a separately running process
// Matthias Wandel 2015
//
// Imgcomp is licensed under GPL v2 (see README.txt)
//----------------------------------------------------------------------------------------
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#ifdef _WIN32
    typedef int pid_t;
    #define execvp(a,b)
    #define fork() 1
#else
    #include <unistd.h>
    #include <sys/wait.h>
#endif

#include "imgcomp.h"
#include "jhead.h"

//"raspistill -q 10 -n -bm -th none -p 480,0,800,480 -w 1280 -h 720 -o /ramdisk/out%05d.jpg -t 4000000 -tl 300";

static int raspistill_pid = 0;
static int blink_led_pid = 0;

//-----------------------------------------------------------------------------------
// Parse command line and launch.
//-----------------------------------------------------------------------------------
static void do_launch_program(char * cmd_string)
{
    char * Arguments[51];
    int narg;
    int a;
    int wasblank = 1;

    // Note: NOT handling quoted strings or anything for arguments with spaces in them.
    narg=0;
    for (a=0;cmd_string[a];a++){
        if (cmd_string[a] != ' '){
            if (wasblank){
                if (narg >= 50){
                    fprintf(stderr, "too many command line arguments\n");
                    exit(0);
                }
                Arguments[narg++] = &cmd_string[a];
            }
            wasblank = 0;
        }else{
            cmd_string[a] = '\0';
            wasblank = 1;
        }
    }
    Arguments[narg] = NULL;

    //printf("%d arguments\n",narg);
    //for (a=0;a<narg;a++){
    //    printf("'%s'\n",Arguments[a]);
    //}
    execvp(Arguments[0], Arguments);

    // execvp only returns if there is an error.

    fprintf(Log,"Failed to execute: %s\n",Arguments[0]);
    perror("Reason");
    exit(errno);
}

//-----------------------------------------------------------------------------------
// Parse command line and launch.
//-----------------------------------------------------------------------------------
static int launch_raspistill(void)
#ifdef _WIN32
{ return 0; }
#else
{
    pid_t pid;
    int ignore;

    // Kill raspistill if it's already running.
    ignore = system("killall -9 raspistill");
    ignore += 1;  // Do something with it to supress warning
    if (raspistill_pid){
        // If we launched raspistill, need to call wait() so that we dont't
        // accumulate an army of child zombie processes
        int exit_code = 123;
        int a;
        time_t then, now = time(NULL);
        a = wait(&exit_code);
        fprintf(Log,"Child exit code %d, wait returned %d",exit_code, a);
        then = time(NULL);
        fprintf(Log," At %02d:%02d (%d s)\n",(int)(then%3600)/60, (int)(then%60), (int)(then-now));
    }

    fprintf(Log,"Launching raspistill program\n");
    pid = fork();
    if (pid == -1){
        // Failed to fork.
        fprintf(Log,"Failed to fork off child process\n");
        fprintf(stderr,"Failed to fork off child process\n");
        perror("Reason");
        return -1;
    }

    if(pid == 0){ 
        // Child takes this branch.
        do_launch_program(raspistill_cmd);
    }else{
        raspistill_pid = pid;
    }
    return 0;
}
#endif


//-----------------------------------------------------------------------------------
// Parse command line and launch.
//-----------------------------------------------------------------------------------
static int MsSinceImage = 0;
static int MsSinceLaunch = 0;
static int InitialAverageBright;
static int InitialBrSum;
static int InitialNumBr;
static int NumTotalImages;
static double RunningAverageBright;

int manage_raspistill(int NewImages)
{
	int timeout;
    MsSinceImage += MsPerCycle;
    MsSinceLaunch += MsPerCycle;
    if (NewImages > 0){
        MsSinceImage = 0;
		NumTotalImages += NewImages;
        if (MsSinceLaunch <= MsPerCycle*2 && BrightnessChangeRestart){
            fprintf(Log,"Exp:%5.1fms Iso:%d  Bright:%d  av=%5.2f\n",
                ImageInfo.ExposureTime*1000, ImageInfo.ISOequivalent, 
                NewestAverageBright, RunningAverageBright);
        }
    }else{
        if (MsSinceImage >= 3000){
            time_t now = time(NULL);
			fprintf(Log,"No new images, %d (at %d:%d)\n",MsSinceImage, (int)(now%3600/60), (int)(now%60));
		}
    }

    if (raspistill_pid == 0){
        // Raspistill has not been launched.
        fprintf(Log,"Initial launch of raspistill\n");
        goto force_restart;
    }

    timeout = 5 * 1000;
    if (MsSinceImage > timeout){
        // Not getting any images for 5 seconds or vide ofiles for 10.
        // Probably something went wrong with raspistill or raspivid.
		if (MsSinceLaunch > timeout){
			if (MsSinceImage > timeout * 3){
				if (NumTotalImages >= 5){
					fprintf(Log,"Relaunch raspistill didn't fix.  Reboot!.  (%d sec since image)\n",MsSinceImage/1000);
					// force rotation of log.
					LogFileMaintain(1);
					MsSinceImage = 0; // dummy for now.
					printf("Reboot now\n");
					system("reboot");
					exit(0);
				}else{
					// Less than 5 images.  Probably left over from last run.
					fprintf(Log,"Raspistill never worked! Give up. %d sec\n",MsSinceImage/1000);
					LogFileMaintain(1);
					// A reboot wouldn't fix this!
					exit(0);
				}
			}else{
				fprintf(Log,"No images for %d sec.  Relaunch raspistill/vid\n",MsSinceImage/1000);
				goto force_restart;
			}
		}
    }

    if (MsSinceLaunch/MsPerCycle > 7200){
        fprintf(Log,"2 hour raspistill relaunch\n");
        goto force_restart;
    }

    if (BrightnessChangeRestart){
        // If brightness of image changes a lot, restart raspistill, because
        // raspistill doesn't normally do running exposure adjustments.
        
        if (MsSinceLaunch > 3000 && InitialNumBr < 4 && NewImages){
            fprintf(Log,"Brightness average in: %d\n",NewestAverageBright);
            InitialBrSum += NewestAverageBright;
            InitialNumBr += 1;
            // Save average brightness and reset averaging.
            if (InitialNumBr == 4){
                InitialAverageBright = (InitialBrSum+2) / 4;
                if (InitialAverageBright == 0) InitialAverageBright = 1; // Avoid division by zero.
                RunningAverageBright = InitialAverageBright;
                fprintf(Log,"Initial brightness average = %d\n",InitialAverageBright);
            }
        }

        // 20 second time constant brightness averaging.
        RunningAverageBright = RunningAverageBright * 0.95 + NewestAverageBright * 0.05;

        // If brightness changes by more than 20%, relaunch, but only if its not too dark.
        if (MsSinceLaunch > 10000 && (RunningAverageBright > 25 || InitialAverageBright > 25)){
            double Ratio;
            Ratio = RunningAverageBright / InitialAverageBright;
            if (Ratio < 1) Ratio = 1/Ratio;
            if (Ratio > 1.2){
                fprintf(Log,"Brightness change by 20%%  (is %d was %d)  Force restart\n",
				       (int)RunningAverageBright,(int)InitialAverageBright);
                goto force_restart;
            }
        }
        // Smarter things to do later:
        // If image too bright and shutter speed is not fastest, launch raspistill
        // if image is too dark and shutter speed is not 1/8, launch raspistill.
    }
    if (SendTriggerSignals){
        // It's possible to run raspistill so it runs continuously and receives
        // trigger signals.  In this mode, it does running exposure adjustments, but
        // only takes pictures when it receives a signal.
        fprintf(Log,"send signal to raspistill (pid=%d)\n",raspistill_pid);
        #ifndef _WIN32
        kill(raspistill_pid, SIGUSR1);
        #endif
    }
    return 0;
    
force_restart:
    launch_raspistill();
    MsSinceLaunch = 0;
    InitialBrSum = InitialNumBr = 0;
    return 1;
}


//-----------------------------------------------------------------------------------
// Run a program to blink the LED.
// Hitting the I/O lines requires root priviledges, so let's just spawn a program
// to do a single LED blink.
//-----------------------------------------------------------------------------------
void run_blink_program()
{
#ifdef _WIN32
  return; }
#else
    pid_t pid;

    if (blink_cmd[0] == 0){
        // No blink command configured.
        return;
    }

    if (blink_led_pid){
        // Avoid accumulating zombie child processes.
        // Blink process should be done by now.
        int exit_code = 0;
        int a;
        a = wait(&exit_code);
        printf("Child exit code %d (wait returned %d)\n",exit_code,a);
        blink_led_pid = 0;
    }


    printf("Run blink program\n");
    pid = fork();
    if (pid == -1){
        // Failed to fork.
        fprintf(stderr,"Failed to fork off child process\n");
        perror("Reason");
        return;
    }

    if(pid == 0){ 
        // Child takes this branch.
        do_launch_program(blink_cmd);
    }else{
        blink_led_pid = pid;
    }
    return;
}
#endif
