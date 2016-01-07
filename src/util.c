//-----------------------------------------------------------------------------------
// imgcomp Various utility functions.
// Matthias Wandel 2015
//
// Imgcomp is licensed under GPL v2 (see README.txt)
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

#ifdef _WIN32
    #include <direct.h> // for mkdir under windows.
    #define mkdir(dir,mode) _mkdir(dir)
    #define S_ISDIR(a)   (a & _S_IFDIR)
    #define PATH_MAX _MAX_PATH
#else
    #ifndef PATH_MAX
        #define PATH_MAX 1024
    #endif
#endif

#include "imgcomp.h"

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


//--------------------------------------------------------------------------------
// Ensure that a path exists
//--------------------------------------------------------------------------------
int EnsurePathExists(const char * FileName)
{
    char NewPath[PATH_MAX*2];
    int a;
    int LastSlash = 0;

    // Extract the path component of the file name.
    strcpy(NewPath, FileName);
    a = strlen(NewPath);
    for (;;){
        a--;
        if (a == 0){
            NewPath[0] = 0;
            break;    
        }
        if (NewPath[a] == '/'){
            struct stat dummy;
            NewPath[a] = 0;
            if (stat(NewPath, &dummy) == 0){
                if (S_ISDIR(dummy.st_mode)){
                    // Break out of loop, and go forward along path making
                    // the directories.
                    if (LastSlash == 0){
                        // Full path exists.  No need to create any directories.
                        return 1;
                    }
                    break;
                }else{
                    // Its a file.
                    fprintf(stderr,"Can't create path '%s' due to file conflict\n",NewPath);
                    return 0;
                }
            }
            if (LastSlash == 0) LastSlash = a;
        }
    }

    // Now work forward.
    //printf("Existing First dir: '%s' a = %d\n",NewPath,a);

    for(;FileName[a];a++){
        if (FileName[a] == '/' || a == 0){
            if (a == LastSlash) break;
            NewPath[a] = FileName[a];
            //printf("make dir '%s'\n",NewPath);
            #ifdef _WIN32
                if (NewPath[1] == ':' && strlen(NewPath) == 2) continue;
            #endif
            //printf("mkdir %s\n",NewPath);
            if (mkdir(NewPath,0777)){
                fprintf(stderr,"Could not create directory '%s'\n",NewPath);
                // Failed to create directory.
                return 0;
            }
        }
    }
    return 1;
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
// Make a name from the file date.
//-----------------------------------------------------------------------------------
void PicDestFromTime(char * DestPath, const char * KeepPixDir, time_t PicTime, char * NameSuffix)
{
    struct tm tm;
    char * p;
    tm = *localtime(&PicTime);

    // %d  Day of month as decimal number (01 – 31)
    // %H	Hour in 24-hour format (00 – 23)
    // %j	Day of year as decimal number (001 – 366)
    // %m	Month as decimal number (01 – 12)
    // %M	Minute as decimal number (00 – 59)
    // %S	Second as decimal number (00 – 59)
    // %U	Week of year as decimal number, with Sunday as first day of week (00 – 53)
    // %w	Weekday as decimal number (0 – 6; Sunday is 0)
    // %y	Year without century, as decimal number (00 – 99)
    // %Y	Year with century, as decimal number

    p = DestPath+sprintf(DestPath, "%s/", KeepPixDir);

    // Using this rule, make a subdiretories for date and for hour.
    p += strftime(p, PATH_MAX, SaveNames, &tm);
    sprintf(p, "%s.jpg",NameSuffix);
}

//-----------------------------------------------------------------------------------
// Back up a photo that is of interest or applies to tiemelapse.
//-----------------------------------------------------------------------------------
char * BackupPicture(char * Name, time_t mtime, int DiffMag)
{
    static char DstPath[500];
    static char SuffixChar = ' ';
    static time_t LastSaveTime;

    if (SaveDir[0] == '\0') return NULL; // Picture saving not enabled.

    {
        char NameSuffix[20];
        if (LastSaveTime == mtime){
            // If it's the same second, cycle through suffixes a-z
            SuffixChar = (SuffixChar >= 'a' && SuffixChar <'z') ? SuffixChar+1 : 'a';
        }else{
            // New time. No need for a suffix.
            SuffixChar = ' ';
            LastSaveTime = mtime;
        }
        sprintf(NameSuffix, "%c%04d",SuffixChar, DiffMag);
        PicDestFromTime(DstPath, SaveDir, mtime, NameSuffix);
        EnsurePathExists(DstPath);
        CopyFile(Name, DstPath);
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



