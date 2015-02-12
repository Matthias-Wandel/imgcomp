//-----------------------------------------------------------------------------------
// Various utility functions.
//-----------------------------------------------------------------------------------
#include <stdio.h>
#include <ctype.h>		// to declare isupper(), tolower() 
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>

#ifdef _WIN32
    #include "readdir.h"
    #define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
    #define strdup(a) _strdup(a) 
    #include <sys/utime.h>
    #define open(a,b,c) _open(a,b,c)
    #define read(a,b,c) _read(a,b,c)
    #define write(a,b,c) _write(a,b,c)
    #define close(a) _close(a)
#else
    #include <dirent.h>
    #include <unistd.h>
    #include <utime.h>
    #define O_BINARY 0

#endif

#include "imgcomp.h"

static time_t NextTimelapsePix;

//-----------------------------------------------------------------------------------
// Concatenate dir name and file name.  Not thread safe!
//-----------------------------------------------------------------------------------
char * CatPath(char *Dir, char * FileName)
{
    static char catpath[502];
    int pathlen;

    pathlen = strlen(Dir);
    if (pathlen > 300){
        fprintf(stderr, "path too long!");
        exit(-1);
    }
    memcpy(catpath, Dir, pathlen+1);
    if (catpath[pathlen-1] != '/' && catpath[pathlen-1] != '\\'){
        catpath[pathlen] = '/';
        pathlen += 1;
    }
    strncpy(catpath+pathlen, FileName,200);
    return catpath;
}

//-----------------------------------------------------------------------------------
// Compare file names to sort directory.
//-----------------------------------------------------------------------------------
static int fncmpfunc (const void * a, const void * b)
{
    return strcmp(*(char **)a, *(char **)b);
}

//-----------------------------------------------------------------------------------
// Read a directory and sort it.
//-----------------------------------------------------------------------------------
char ** GetSortedDir(char * Directory, int * NumFiles)
{
    char ** FileNames;
    int NumFileNames;
    int NumAllocated;
    DIR * dirp;

    NumAllocated = 5;
    FileNames = malloc(sizeof (char *) * NumAllocated);

    NumFileNames = 0;
  
    dirp = opendir(Directory);
    if (dirp == NULL){
        fprintf(stderr, "could not open dir\n");
        return NULL;
    }

    for (;;){
        struct dirent * dp;
        struct stat buf;
        int l;
        dp = readdir(dirp);
        if (dp == NULL) break;
        //printf("name: %s %d %d\n",dp->d_name, (int)dp->d_off, (int)dp->d_reclen);


        // Check that name ends in ".jpg", ".jpeg", or ".JPG", etc...
        l = strlen(dp->d_name);
        if (l < 5) continue;
        if (dp->d_name[l-1] != 'g' && dp->d_name[l-1] != 'G') continue;
        if (dp->d_name[l-2] == 'e' || dp->d_name[l-2] == 'E') l-= 1;
        if (dp->d_name[l-2] != 'p' && dp->d_name[l-2] != 'P') continue;
        if (dp->d_name[l-3] != 'j' && dp->d_name[l-3] != 'J') continue;
        if (dp->d_name[l-4] != '.') continue;
        //printf("use: %s\n",dp->d_name);

        // Check that it's a regular file.
        stat(CatPath(Directory, dp->d_name), &buf);
        if (!S_ISREG(buf.st_mode)) continue; // not a file.

        if (NumFileNames >= NumAllocated){
            //printf("realloc\n");
            NumAllocated *= 2;
            FileNames = realloc(FileNames, sizeof (char *) * NumAllocated);
        }

        FileNames[NumFileNames++] = strdup(dp->d_name);
    }
    closedir(dirp);

    // Now sort the names (could be in random order)
    qsort(FileNames, NumFileNames, sizeof(char **), fncmpfunc);

    *NumFiles = NumFileNames;
    return FileNames;
}

//-----------------------------------------------------------------------------------
// Unallocate directory structure
//-----------------------------------------------------------------------------------
void FreeDir(char ** FileNames, int NumEntries)
{
    int a;
    // Free it up again.
    for (a=0;a<NumEntries;a++){
        free(FileNames[a]);
        FileNames[a] = NULL;
    }
    free(FileNames);
}

//-----------------------------------------------------------------------------------
// Back up a photo that is of interest or applies to tiemelapse.
//-----------------------------------------------------------------------------------
char * BackupPicture(char * Directory, char * Name, char * KeepPixDir, int Threshold, int MotionTriggered)
{
    char SrcPath[500];
    static char DstPath[500];
    struct stat statbuf;
    static char ABCChar = ' ';
    static time_t LastSaveTime;
    struct tm tm;


    if (!KeepPixDir) return NULL; // Picture saving not enabled.

    strcpy(SrcPath, CatPath(Directory, Name));
    if (stat(SrcPath, &statbuf) == -1) {
        perror(SrcPath);
        exit(1);
    }
    if (!MotionTriggered){
        if (TimelapseInterval < 1) return 0; // timelapse mode off.
        if (statbuf.st_mtime < NextTimelapsePix){
            // No motion, not timelapse picture.
            return NULL;
        }
    }

    if (TimelapseInterval >= 1){
        // Figure out when the next timelapse interval should be.
        NextTimelapsePix = statbuf.st_mtime+TimelapseInterval;
        NextTimelapsePix -= (NextTimelapsePix % TimelapseInterval);
    }

    if (FollowDir){
        // In followdir mode, rename according to date.
         if (stat(SrcPath, &statbuf) == -1) {
            perror(SrcPath);
            exit(1);
        }
        if (LastSaveTime == statbuf.st_mtime){
            // If it's the same second, cycle through suffixes a-z
            ABCChar = (ABCChar >= 'a' && ABCChar <'z') ? ABCChar+1 : 'a';
        }else{
            // New time. No need for a suffix.
            ABCChar = ' ';
            LastSaveTime = statbuf.st_mtime;
        }

        tm = *localtime(&statbuf.st_mtime);
        sprintf(DstPath, "%s/%02d%02d-%02d%02d%02d%c%04d.jpg",KeepPixDir,
             tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, 
             ABCChar, Threshold);

        CopyFile(SrcPath, DstPath);
    }else{
        // In test mode, just reuse the name.
        CopyFile(SrcPath, CatPath(KeepPixDir, Name));
    }
    return DstPath;
}


//-----------------------------------------------------------------------------------
// Copy a file.
//-----------------------------------------------------------------------------------
int CopyFile(char * src, char * dest)
{
    int inputFd, outputFd, openFlags;
    int filePerms;
    struct stat statbuf;
    size_t numRead;
    #define BUF_SIZE 8192
    char buf[BUF_SIZE];

    //printf("Copy %s --> %s\n",src,dest);

    // Get file modification time from old file.
    stat(src, &statbuf);

    // Open input and output files  
    inputFd = open(src, O_RDONLY | O_BINARY, 0);
    if (inputFd == -1){
        fprintf(stderr,"CopyFile could not open src %s\n",src);
        exit(-1);
    }
 
    openFlags = O_CREAT | O_WRONLY | O_TRUNC | O_BINARY;

    filePerms = 0x1ff;

    outputFd = open(dest, openFlags, filePerms);
    if (outputFd == -1){
        fprintf(stderr,"CopyFile coult not open dest %s\n",dest);
        exit(-1);
    }

    // Transfer data until we encounter end of input or an error
 
    for(;;){
        numRead = read(inputFd, buf, BUF_SIZE);
        if (numRead <= 0) break;
        if (write(outputFd, buf, numRead) != numRead){
            fprintf(stderr,"write error to %s",dest);
            exit(-1);
        }
    }

    if (numRead == -1){
        fprintf(stderr,"CopyFile read error from %s\n",src);
        exit(-1);
    }

    close(inputFd);
    close(outputFd);

    {
        struct utimbuf mtime;
        mtime.actime = statbuf.st_ctime;
        mtime.modtime = statbuf.st_mtime;
        utime(dest, &mtime);
    }
    
    return 0;
}



