//----------------------------------------------------------------------------------------
// Code to launch or relaunch raspistill as a separately program.
// monitors that raspistill is still producing images, restarts it if it stops.
// Matthias Wandel 2015
//
// Imgcomp is licensed under GPL v2 (see README.txt)
//----------------------------------------------------------------------------------------
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>

#include "imgcomp.h"
#include "config.h"
#include "jhead.h"

static int raspistill_pid = 0;
static int blink_led_pid = 0;

static char OutNameSeq = 'a';

int relaunch_timeout = 6;
int give_up_timeout = 18;

int kill(pid_t pid, int sig);
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
// Launch or re-launch raspistill.
//-----------------------------------------------------------------------------------
int relaunch_raspistill(void)
{
    pid_t pid;

    // Kill raspistill if it's already running.
    if (raspistill_pid){
        kill(raspistill_pid,SIGKILL);
        // If we launched raspistill, need to call wait() so that we dont't
        // accumulate an army of child zombie processes
        int exit_code = 123;
        int a;
        time_t then, now = time(NULL);
        a = wait(&exit_code);
        fprintf(Log,"Child exit code %d, wait returned %d",exit_code, a);
        then = time(NULL);
        fprintf(Log," At %02d:%02d (%d s)\n",(int)(then%3600)/60, (int)(then%60), (int)(then-now));
    } else {
        // Original way of killing raspistill, still used if it was launched externally
        // and we don't have the pid.  Cannot be used with capture programs other than raspistill
        (void) system("killall -9 raspistill");
    }

    fprintf(Log,"Launching raspistill program\n");

    int DashOOption = (strstr(raspistill_cmd, " -o ") != NULL);

    static char cmd_appended[300];
    strncpy(cmd_appended, raspistill_cmd, 200);

    if (ExposureManagementOn) { // Exposure managemnt by imgcomp
        strcat(cmd_appended, GetRaspistillExpParms());
        if (DashOOption){
            fprintf(stderr, "Must not specify -o option with -exm option\n");
            exit(-1);
        }
    }

    if (!DashOOption){
        // No output specified with raspistill command  Add the option,
        // with a different prefix each time so numbers don't overlap.
        int l = strlen(cmd_appended);
        sprintf(cmd_appended+l," -o %s/out%c%%05d.jpg",DoDirName, OutNameSeq++);
        if (OutNameSeq >= 'z') OutNameSeq = 'a';
        printf("Run program: %s\n",cmd_appended);
    }

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
        do_launch_program(cmd_appended);
    }else{
        raspistill_pid = pid;
    }

    return 0;
}

//-----------------------------------------------------------------------------------
// Manage raspistill - may need restarting if it dies or brightness changed too much.
//-----------------------------------------------------------------------------------
static int MsSinceImage = 0;
static int MsSinceLaunch = 0;
static int InitialBrSum;
static int InitialNumBr;
static int NumTotalImages;

int manage_raspistill(int NewImages)
{
    int timeout;
    MsSinceImage += 1000;
    MsSinceLaunch += 1000;
    if (NewImages > 0){
        MsSinceImage = 0;
		NumTotalImages += NewImages;
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

    timeout = relaunch_timeout * 1000;
    if (MsSinceImage > timeout){
        // Not getting any images for 5 seconds or vide ofiles for 10.
        // Probably something went wrong with raspistill or raspivid.
		if (MsSinceLaunch > timeout){
			if (give_up_timeout && MsSinceImage > give_up_timeout * 1000){
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
    return 0;

force_restart:
    relaunch_raspistill();
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
