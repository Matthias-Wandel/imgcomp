//----------------------------------------------------------------------------------
// HTML based image browser to use with imgcomp output.
//
// Small CGI program to watch log file and wait for significant enough change
// to trigger an image update in realtime mode.
//
// Imgcomp and html browsing tool is licensed under GPL v2 (see README.txt)
//----------------------------------------------------------------------------------
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/inotify.h>

//----------------------------------------------------------------------------------
// 
//----------------------------------------------------------------------------------
int main()
{
    int n, GotBytes = 0;
    FILE * LogFile;
    long FileSize;
    
    const char *WATCH_FILE = "/ramdisk/log.txt";
    
    printf("Content-Type: text/html\n\n<html>\n"); // html header
    
    LogFile = fopen(WATCH_FILE, "r");
    if (LogFile == NULL){
        printf("Error!  No log.txt file\n");
        sleep(1); // To keep from cycling too fast.
        return 0;
    }
    fseek(LogFile, 0, SEEK_END);
    FileSize = ftell(LogFile);
    
    int infd;
    const int EVENT_BUF_LEN = 100;
    char buffer[EVENT_BUF_LEN];
    infd = inotify_init();
    inotify_add_watch( infd, WATCH_FILE, IN_CREATE | IN_DELETE | IN_MODIFY);
    
    for (n=0;n<20;n++){
        char NewBytes[100];
        int nread;
        long NewSize;
        
        // read to wait for change.
        read( infd, buffer, EVENT_BUF_LEN ); 

        // the following hack is necessary to work around a file system bug on the
        // raspian for raspberry pi 4.
        fseek(LogFile, 0, SEEK_END);
        NewSize = ftell(LogFile);
        if (NewSize != FileSize){
            // Go back to read what was appended
            fseek(LogFile, FileSize, SEEK_SET);
            FileSize = NewSize;
        }else{
            // File wasn't appended to.
            continue;
        }
        
        nread = fread(NewBytes, 1, 99, LogFile);
        if (nread > 0){
            GotBytes = 1;
            NewBytes[nread] = 0;
            printf("%s<br>",NewBytes);
            if (strstr(NewBytes, "(")){
                // Saw enough motion to show location (though not necessarily enough to save image).
                // Enouch change for live mode update.
                break;
            }
        }
    }
    
    if (GotBytes == 0){
        printf("Error! log.txt not updating\n");
    }
    
    // If more than 2 seconds without change, print error messages.
    // Extract motion level, if it's above some threshold, call it motion (use cofig file?)
    return 0;
}