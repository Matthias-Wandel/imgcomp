#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <poll.h>

#define EVENT_BUF_LEN 1000
int main( int argc, char **argv )
{
    int length, i = 0;
    int fd;
    int wd;
    char buffer[EVENT_BUF_LEN];
    
    // create INOTIFY instance
    fd = inotify_init();
    
    //checking for error
    if (fd<0) perror("inotify_init");
    
    wd = inotify_add_watch( fd, "/ramdisk", IN_CREATE);

for(;;){    

    
    struct pollfd pfd = { fd, POLLIN, 0 };
    int ret = poll(&pfd, 1, 500);  // timeout of 100 ms
    printf("poll ret = %d\n",ret);
    if (ret < 0) {
        printf("select failed: %s\n", strerror(errno));
    } else if (ret == 0) {
        // Timeout with no events, move on.
        printf("timeout, no events\n");
        continue;
    } else {
        // Process the new event.
        struct inotify_event event;
        int nbytes = read(fd, &event, sizeof(event));
        // Do what you need...
    }
    
    

    // read to determine the event change
    length = read( fd, buffer, EVENT_BUF_LEN ); 
    
    if (length<0) perror("read");
    
    printf("-------------------------------\nlength = %d\n",length);
    // Read the return list
    i=0;
    while (i< length) {
        struct inotify_event *event = (struct inotify_event* ) &buffer[ i ];
        if ( event->len ) {
            if ( event->mask & IN_CREATE ) {
                if ( event->mask & IN_ISDIR ) {
                    printf( "New directory %s created.\n", event->name );
                }
                else {
                    printf( "New file %s created.\n", event->name );
                }
            }
            else if ( event->mask & IN_DELETE ) {
                if ( event->mask & IN_ISDIR ) {
                    printf( "Directory %s deleted.\n", event->name );
                } else {
                    printf( "File %s deleted.\n", event->name );
                }
            }else{
                printf("mask %x\n",event->mask);
            }
        }else{
            printf("length = %d\n",event->len);
            
        }
        i +=  sizeof (struct inotify_event) + event->len;
        break;
    }
}
   inotify_rm_watch( fd, wd );

   close( fd );
}
   
   
   
//----------------------------------------------------------------------------------
// 
//----------------------------------------------------------------------------------
int xmain()
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