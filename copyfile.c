#include <stdio.h>
#include <ctype.h>		// to declare isupper(), tolower() 
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>

#ifdef _WIN32
    #include <io.h>
    #include <sys/utime.h>
    #define S_IRUSR 0
    #define S_IWUSR 0
    #define S_IRGRP 0
    #define S_IWGRP 0
    #define open(a,b,c) _open(a,b,c)
    #define read(a,b,c) _read(a,b,c)
    #define write(a,b,c) _write(a,b,c)
    #define close(a) _close(a)
#else
    #include <unistd.h> 
    #define O_BINARY 0
#endif
#include "imgcomp.h"

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

    // Open input and output files 
    printf("Copy %s --> %s\n",src,dest);

    // Get file modification time from old file.
    stat(src, &statbuf);
 
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
        int a;
        mtime.actime = statbuf.st_ctime;
        mtime.modtime = statbuf.st_mtime;
        a = utime(dest, &mtime);
        printf("set to %d ret %d\n",statbuf.st_ctime, a);
    }
    
    return 0;
}



