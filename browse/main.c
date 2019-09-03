//----------------------------------------------------------------------------------
// makehtml main program
// Coordinates making of html index files.
// November 1999 - Sept 2006
//---------------------------------------------------------------------------------- 
#include <stdio.h>
#include <errno.h>
#include <memory.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include "view.h"
#ifdef _WIN32
    #include <io.h>
    #include <direct.h>
#else
    #include <sys/stat.h>
    #include <unistd.h>
#endif

static char * ImageExtensions[] = {"jpg","jpeg","c",NULL};

//----------------------------------------------------------------------------------
// Process one directory.  Returns pointer to summary.
//----------------------------------------------------------------------------------
Dir_t * CollectDir(char * HtmlPath)
{
    Dir_t * Dir;
    VarList * Subdirs;
    VarList * Images;
	char DirName[300];

    Dir = (Dir_t *) malloc(sizeof(Dir_t));
    memset(Dir, 0, sizeof(Dir_t));

    if (Dir == NULL){
        printf("out of memory\n");
        // Very bad.  Don't attempt to fix it.
        exit(-1);
    }

    Subdirs = &Dir->Dirs;
    Images = &Dir->Images;

    if (strlen(HtmlPath) > 250){
        printf("Path string is too long\n");
        exit(0);
    }
	strcpy(Dir->HtmlPath, HtmlPath);

	sprintf(DirName, "pix/%s",HtmlPath);
	CollectDirectory(DirName, Images, Subdirs, ImageExtensions);
   

	// Look for previous and next.
	if (strlen(HtmlPath) > 2){
		char ThisDir[100];
		VarList Siblings;
		int a;
		int LastSlash = 0;
		Siblings.NumEntries = Siblings.NumAllocated = 0;
		Siblings.Entries = NULL;
		
		for (a=0;HtmlPath[a] && a < 99;a++){
			if (HtmlPath[a-1] == '/' && HtmlPath[a]) LastSlash = a;
		}
		memcpy(Dir->Parent, HtmlPath, LastSlash);
		Dir->Parent[LastSlash-1] = '\0';
		strcpy(ThisDir, HtmlPath+LastSlash);
		a = strlen(ThisDir);
		if (ThisDir[a-1] == '/') ThisDir[a-1] = '\0';
				
		//printf("<a href=\"view.cgi?%s\">[Parent:%s]</a><p>\n",Dir->Parent,Dir->Parent);
		
		sprintf(DirName, "pix/%s",Dir->Parent);
		
		CollectDirectory(DirName, NULL, &Siblings, NULL);
		//printf("<p>%d siblings to %s  %s<p>",Siblings.NumEntries, DirName, ThisDir);
		
		for (a=0;a<Siblings.NumEntries;a++){
			char * slash;
			//printf("%s<br>\n",Siblings.Entries[a].Name);
			slash = "";
			if (Dir->Parent[0]) slash = "/";
			if (strcmp(Siblings.Entries[a].Name, ThisDir) == 0){
				if (a > 0){
					sprintf(Dir->Previous, "%s%s%s", Dir->Parent, slash, Siblings.Entries[a-1].Name);
				}
				if (a < Siblings.NumEntries-1){
					sprintf(Dir->Next, "%s%s%s", Dir->Parent, slash, Siblings.Entries[a+1].Name);
				}
				//printf("Here!");
			}
		}
		
	}
		
    return Dir;
}

//----------------------------------------------------------------------------------
// Main
//----------------------------------------------------------------------------------
int main(int argc, char ** argv)
{
    int a;
	char * qenv;
	char HtmlPath[300];
	qenv = getenv("QUERY_STRING");	
	
	HtmlPath [0] = '\0';
	if (qenv){
		printf("Content-Type: text/html\n\n<html>\n"); // html header
		strcpy(HtmlPath, qenv);
	}

    // Parse the command line options.
    for (a=1;a<argc;a++){
        if (argv[a][0] == '-'){
            if (strcmp(argv[a],"-S") == 0){
                // Recurse subdirectories (default)
                //DoRecursive = TRUE;
            }else if (strcmp(argv[a],"-ns") == 0){
                // Do not recurse subdirectories
                //DoRecursive = FALSE;
            }else{
                fprintf(stderr,"Error: Argument '%s' not understood\n",argv[a]);
            }
        }else{
            // Plain argument with no dashes means root path.
			strcpy(HtmlPath, argv[a]);
        }
    }
	//printf("QUERY_STRING=%s<br>\n",qenv);

	{
		int l;
		l = strlen(HtmlPath);
		if (l && HtmlPath[l-1] != '/'){
			HtmlPath[l] = '/';
			HtmlPath[l+1] = '\0';
		}
	}
	
	{
		Dir_t * Col;
		Col = CollectDir(HtmlPath);
		MakeHtmlOutput(Col);
	}
    return 0;
}

