#include <stdio.h>
#include <unistd.h>
#include <string.h>

//----------------------------------------------------------------------------------
// 
//----------------------------------------------------------------------------------
int main()
{
    int n, GotBytes = 0;
    FILE * LogFile;
    long FileSize;
    
    printf("Content-Type: text/html\n\n<html>\n"); // html header
    
    LogFile = fopen("/ramdisk/log.txt", "r");
    if (LogFile == NULL){
        printf("Error!  No log.txt file\n");
        sleep(1); // To keep from cycling too fast.
        return 0;
    }
    fseek(LogFile, 0, SEEK_END);
    FileSize = ftell(LogFile);
    
    for (n=0;n<30;n++){
        char NewBytes[100];
        int nread;
        long NewSize;
        usleep(100000);

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