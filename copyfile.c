#include <stdio.h>
#include <ctype.h>		// to declare isupper(), tolower() 
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>

#ifdef _WIN32
    #include <io.h>
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
    size_t numRead;
    #define BUF_SIZE 8192
    char buf[BUF_SIZE];

    // Open input and output files 
 
    inputFd = open(src, O_RDONLY | O_BINARY, 0);
    if (inputFd == -1){
        fprintf(stderr,"CopyFile coult not open src %s\n",src);
        exit(-1);
    }
 
    openFlags = O_CREAT | O_WRONLY | O_TRUNC | O_BINARY;

    filePerms = 0x1ff;//S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;

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
    
    return 0;
}







/*
int copy_data_buffer(int fdin, int fdout, void *buf, size_t bufsize) {
    for(;;) {
       void *pos;
       // read data to buffer
       ssize_t bytestowrite = read(fdin, buf, bufsize);
       if (bytestowrite == 0) break; // end of input
       if (bytestowrite == -1) {
           if (errno == EINTR) continue; // signal handled
           if (errno == EAGAIN) {
               block(fdin, POLLIN);
               continue;
           }
           return -1; // error
       }

       // write data from buffer
       pos = buf;
       while (bytestowrite > 0) {
           ssize_t bytes_written = write(fdout, pos, bytestowrite);
           if (bytes_written == -1) {
               return -1; // error
           }
           bytestowrite -= bytes_written;
           pos += bytes_written;
       }
    }
    return 0; // success
}

// Default value. I think it will get close to maximum speed on most
// systems, short of using mmap etc. But porters / integrators
// might want to set it smaller, if the system is very memory
// constrained and they don't want this routine to starve
// concurrent ops of memory. And they might want to set it larger
// if I'm completely wrong and larger buffers improve performance.
// It's worth trying several MB at least once, although with huge
// allocations you have to watch for the linux 
// "crash on access instead of returning 0" behaviour for failed malloc.
#ifndef FILECOPY_BUFFER_SIZE
    #define FILECOPY_BUFFER_SIZE (64*1024)
#endif

int copy_data(int fdin, int fdout) {
    // optional exercise for reader: take the file size as a parameter,
    // and don't use a buffer any bigger than that. This prevents 
    // memory-hogging if FILECOPY_BUFFER_SIZE is very large and the file
    // is small.
    for (size_t bufsize = FILECOPY_BUFFER_SIZE; bufsize >= 256; bufsize /= 2) {
        void *buffer = malloc(bufsize);
        if (buffer != NULL) {
            int result = copy_data_buffer(fdin, fdout, buffer, bufsize);
            free(buffer);
            return result;
        }
    }
    // could use a stack buffer here instead of failing, if desired.
    // 128 bytes ought to fit on any stack worth having, but again
    // this could be made configurable.
    return -1; // errno is ENOMEM
}
*/