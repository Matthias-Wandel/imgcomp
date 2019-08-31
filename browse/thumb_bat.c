//----------------------------------------------------------------------------------
// makehtml main program
// Coordinates making of html index files.
// November 1999 - Sept 2006
//---------------------------------------------------------------------------------- 
#include <stdio.h>
#include <errno.h>
#include <memory.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

//----------------------------------------------------------------------------------
// Copy a picture file (for image magic to manipulate it in place)
//----------------------------------------------------------------------------------
void FileToStdOut(char * SrcPath)
{
    static unsigned char BigBuf[20000];
    FILE * Rd;

    //printf("copy from:%s\n to:%s\n",SrcPath, DestPath);

    Rd = fopen(SrcPath, "rb");
    if (Rd == NULL){
        printf("could not read '%s'\n", SrcPath);
        exit(-1);
    }

    for(;;){
        int bytes;
        bytes = fread(BigBuf, 1, sizeof(BigBuf),Rd);
        if (bytes <= 0) break;
        fwrite(BigBuf, 1, bytes,stdout);
    }
    fclose(Rd);
}

//----------------------------------------------------------------------------------
// Main
//----------------------------------------------------------------------------------
int main(int argc, char ** argv)
{
    int a,b;
	char * qenv;
	char ExecString[200];
	char FileName[100];
	char TmpName[50];
	
	printf("Content-Type: image/jpg\n\n"); // html header
	
	qenv = getenv("QUERY_STRING");	
	
	if (qenv == NULL){
		printf("No query string\n");
		exit(0);
	}
	for (a=b=0;a<100;a++){
		if (qenv[a] == '%' && qenv[a+1] == '2' && qenv[a+2] == '0'){
			FileName[b++] = ' ';
			a += 2;
		}else{
			FileName[b++] = qenv[a];
		}
		if (qenv[a] == '\0') break;
	}
	
	sprintf(TmpName, "/ramdisk/%d",getpid());

    sprintf(ExecString, "epeg -w 320 -h 240 \"%s\" %s",FileName,TmpName);
    system(ExecString);
	
	FileToStdOut(TmpName);
	
	unlink(TmpName);
}

