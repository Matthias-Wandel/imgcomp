//----------------------------------------------------------------------------------------
// Code to launch raspistill as a separately running process
//----------------------------------------------------------------------------------------
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "imgcomp.h"
#include "jhead.h"

// Should later read these from a file, maybe.
//static char raspistill_cmd[] = "raspistill -q 10 -n -bm -th none -p 480,0,800,480 -awb off -w 1280 -h 720 -o /ramdisk/out%05d.jpg -t 1800000 -tl 300";

static char raspistill_cmd[] = "raspistill -q 10 -th none -w 1280 -h 720 -o /ramdisk/out%05d.jpg -t 1800000 -tl 300";

static int raspistill_pid = 0;

//-----------------------------------------------------------------------------------
// Parse command line and launch.
//-----------------------------------------------------------------------------------
static void do_launch_program(void)
{
    char * Arguments[51];
    int narg;
    int a;
    int wasblank = 1;

    // Note: NOT handling quoted strings or anything for arguments with spaces in them.
    narg=0;
    for (a=0;raspistill_cmd[a];a++){
        if (raspistill_cmd[a] != ' '){
            if (wasblank){
                if (narg >= 50){
                    fprintf(stderr, "raspistill too many raspistill arguments\n");
                    exit(0);
                }
                Arguments[narg++] = &raspistill_cmd[a];
            }
            wasblank = 0;
        }else{
            raspistill_cmd[a] = '\0';
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

    fprintf(stderr,"Failed to execute: %s\n",Arguments[0]);
    perror("Reason");
    exit(errno);
}

//-----------------------------------------------------------------------------------
// Parse command line and launch.
//-----------------------------------------------------------------------------------
int launch_raspistill(void)
{
    pid_t pid;

    // Kill raspistill if it's already running.
    system("killall raspistill");

    printf("Launching raspistill program\n");
    pid = fork();
    if (pid == -1){
        // Failed to fork.
        fprintf(stderr,"Failed to fork off child process\n");
        perror("Reason");
        return -1;
    }

    if(pid == 0){ 
        // Child takes this branch.
        do_launch_program();
    }else{
        raspistill_pid = pid;
        printf("must have worked!!! %d\n", pid);
        printf("must have worked!!!\n");
        printf("must have worked!!!\n");
        printf("must have worked!!!\n");
    }
    return 0;
}




//-----------------------------------------------------------------------------------
// Parse command line and launch.
//-----------------------------------------------------------------------------------
int SecondsSinceImage = 0;
int SecondsSinceLaunch = 0;

void manage_raspistill(int NewImages)
{
    SecondsSinceImage += 1;
    SecondsSinceLaunch += 1;
    if (NewImages){
        SecondsSinceImage = 0;
        printf("Shutter speed:%5.1f ms\n",ImageInfo.ExposureTime*1000);
    }

    if (raspistill_pid == 0){
        // Raspistill has not been launched.
        printf("Initial launch of raspistill\n");
        goto force_restart;
    }

    if (SecondsSinceImage > 10){
        // Not getting any images for 10 seconds.  Probably somethign went wrong with raspistill.
        printf("No images timeout.  Relaunch  raspistill\n");
        goto force_restart;
    }

    if (SecondsSinceLaunch > 20){
        printf("Periodic raspistill relaunch\n");
        goto force_restart;
    }

    // If image too bright and shutter speed is not fastest, launch raspistill
    // if image is too dark and shutter speed is not 1/8, launch raspistill.


    return;
force_restart:
    launch_raspistill();
    SecondsSinceImage = 0;
    SecondsSinceLaunch = 0;
}