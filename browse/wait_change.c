//----------------------------------------------------------------------------------
// Small CGI program to watch log file and wait for significant enough change
// to trigger an image update in realtime mode.
//
// Imgcomp and html browsing tool is licensed under GPL v2 (see README.txt)
//----------------------------------------------------------------------------------
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <poll.h>

int RefreshEveryFrame = 0;
//----------------------------------------------------------------------------------
// If there is no log.txt created by imgcomp (none configured), then
// look for new jpg files to appear in the directory we read from instead.
//----------------------------------------------------------------------------------
int WaitForNewFile(char * Dir)
{
    int fd = inotify_init();
    if (fd < 0) perror("inotify_init");

    // raspistill creates the file first with a name such as img1234.jpg~
    // After writing the file, it's renamed to img1234.jpg.  It would make sense to
    // also wait for IN_CLOSE_WRITE in case files are just copied there, however,
    // that will trigger on IN_CLOSE_WRITE even, and by the next time we poll,
    // the image will already be move,d so we miss it.

    inotify_add_watch( fd, "/ramdisk", IN_CLOSE_WRITE | IN_MOVED_TO);

    // Wait for more files to appear.
    struct pollfd pfd = { fd, POLLIN, 0 };
    union {
        struct inotify_event iev;
        char padding [ sizeof(struct inotify_event)+100];
    }ievc;

    for (;;){
        int ret = poll(&pfd, 1, 5000);
        if (ret < 0) {
            printf("Error: select failed: %s\n", strerror(errno));
            return 0;
        }

        if (ret == 0){
            // Timeout waiting for a new file.
            printf("Error: No new images\n");
            return 0;
        }
        //printf("revents=%d\n",pfd.revents);

        ievc.iev.len = 50;
        read(fd, &ievc, sizeof(ievc));

        int nlen = strlen(ievc.iev.name);
        if (nlen > 4 && strcmp(ievc.iev.name+nlen-4, ".jpg") == 0){
            break;
        }else{
            printf("%s\n", ievc.iev.name);
        }
    }

    if (RefreshEveryFrame){
        printf("%s\n", ievc.iev.name);
    }else{
        printf("Please ensure /ramdisk/log.txt is enabled in imgcomp.conf\n");
    }
    return 0;
}


//----------------------------------------------------------------------------------
// Main function
//----------------------------------------------------------------------------------
int main()
{
    int n, GotBytes = 0;
    FILE * LogFile;
    long FileSize;

    char *WATCH_FILE;

    printf("Content-Type: text/html\n\n<html>\n"); // html header

    char * QueryString = getenv("QUERY_STRING");
    if (QueryString && QueryString[0] == '1'){
        RefreshEveryFrame = 1;
    }

    // First try to follow symlink "in" in the current directory.  Only if that fails,
    // use the hardcoded path to ramdisk.
    // I should really put this configuration in browse.cfg
    WATCH_FILE = "in/log.txt";
    LogFile = fopen(WATCH_FILE,"r");

    if (RefreshEveryFrame && LogFile == NULL){
        // Just look for new files coming into existence, we aren't
        // looking for motion.
        return WaitForNewFile("/ramdisk");
    }

    if (LogFile == NULL) {
        WATCH_FILE = "/ramdisk/log.txt";
        LogFile = fopen(WATCH_FILE, "r");
    }

    if (LogFile == NULL){
        sleep(1);
        printf("No /ramdisk/log.txt found.  Please enable in imgcomp.conf\n");
        return 0;
    }

    fseek(LogFile, 0, SEEK_END);
    FileSize = ftell(LogFile);

    int infd;
    const int EVENT_BUF_LEN = 100;
    char buffer[EVENT_BUF_LEN];
    infd = inotify_init();
    inotify_add_watch( infd, WATCH_FILE, IN_CREATE | IN_DELETE | IN_MODIFY);

    time_t now = time(NULL);
    for (n=0;n<20;n++){
        char NewBytes[100];
        int nread;

        if (time(NULL) >= now+5) break; // Wait at most five seconds.

        // poll to make sure a change is ready so the following read doesn't
        // get stuck if the log file isn't being updated.
        struct pollfd pfd = { infd, POLLIN, 0 };
        int ret = poll(&pfd, 1, 1000);
        if (ret == 0){
            // no change for one second.
            continue;
        }

        read( infd, buffer, EVENT_BUF_LEN );

        // the following hack is necessary because just reading from the end of the file again
        // doesn't return anything even if the file grew since the last read.
        fseek(LogFile, 0, SEEK_END);
        long NewSize = ftell(LogFile);
        if (NewSize != FileSize){
            // Go back to read what was appended
            fseek(LogFile, FileSize, SEEK_SET);
            FileSize = NewSize;
        }else{
            continue; // File wasn't appended to.
        }

        nread = fread(NewBytes, 1, 99, LogFile);
        if (nread > 0){
            GotBytes = 1;
            NewBytes[nread] = 0;
            printf("%s<br>",NewBytes);
            if (strstr(NewBytes, "(") || RefreshEveryFrame){
                // Saw enough motion to show location (though not necessarily enough to save image).
                // Enouch change for live mode update.
                break;
            }
        }
    }

    // After 5 seconds, if nothing written to log file, its an error.
    // If stuff was written to log file, stop regardless (so javascript returns and image refreshes anyway)

    if (GotBytes == 0){
        // If more than 5 seconds without change, print error messages.
        // Extract motion level, if it's above some threshold, call it motion (use cofig file?)
        printf("Error! log.txt not updating.  Imgcomp not running?\n");
    }

    return 0;
}
