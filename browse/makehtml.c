//----------------------------------------------------------------------------------
// HTML output generator part of my HTML thumbnail tree building program.
//---------------------------------------------------------------------------------- 
#include <stdio.h>
#include <errno.h>
#include <memory.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#ifdef _WIN32
    #include <io.h>
    #include <direct.h>
#endif

#include "view.h"

//----------------------------------------------------------------------------------
// Create browsable HTML index files for the directories.
//----------------------------------------------------------------------------------
void MakeHtmlOutput(Dir_t * Dir)
{
    VarList Images;
    VarList Directories;

    FILE * HtmlFile;
    char HtmlFileName[500];
    unsigned int a;
	int SkipFactor;

    Images = Dir->Images;
    Directories = Dir->Dirs;    

    //CombinePaths(HtmlFileName, Dir->FullPath, Dir->IndexName);

	HtmlFile = stdout;
    //HtmlFile = fopen(HtmlFileName, "w");
    if (HtmlFile == NULL){
       printf("Could not open '%s' for write\n",HtmlFileName);
        exit(-1);
    }

    // Header of file.
    fprintf(HtmlFile, "<html>\n");
	
	fprintf(HtmlFile, 
		"<style type=text/css>\n"
		"  body { font-family: sans-serif; font-size: 24;}\n"
		"  img { vertical-align: middle; margin-bottom: 5px; }\n"
		"</style></head>\n");

    for (a=0;a<Directories.NumEntries;a++){
		fprintf(HtmlFile, "<a href=\"view.cgi?%s%s\">%s</a>\n",Dir->HtmlPath, Directories.Entries[a].Name, Directories.Entries[a].Name);
		if ((a & 3) == 3) fprintf(HtmlFile, "<br>\n"); // Put two links per line.
    }
	
	fprintf(HtmlFile,"<p>\n");
	if (Images.NumEntries){
		fprintf(HtmlFile,"%s: %d Images<p>\n",Dir->HtmlPath, Images.NumEntries);
	}
	// If there are a LOT of images, don't show all of them!
	SkipFactor = 1;
	if (Images.NumEntries > 40) SkipFactor = 2;
	if (Images.NumEntries > 80) SkipFactor = 3;
	if (Images.NumEntries > 200) SkipFactor = 4;
	if (Images.NumEntries > 300) SkipFactor = 5;

    for (a=0;a<Images.NumEntries;a++){
		fprintf(HtmlFile, "<a href=\"pix/%s%s\">",Dir->HtmlPath, Images.Entries[a].Name);
		if (a % SkipFactor == 0){
			//fprintf(HtmlFile, "<img src=\"pix/%s%s\"",Dir->HtmlPath, Images.Entries[a].Name);		
			fprintf(HtmlFile, "<img src=\"thumb.cgi?pix/%s%s\"",Dir->HtmlPath, Images.Entries[a].Name);
			fprintf(HtmlFile, " width=320 height=240>");
		}else{
			fprintf(HtmlFile, "<big><big>#</big></big>");
		}
		fprintf(HtmlFile, "</a>\n");
    }

   
    if (HtmlFile != stdout) fclose(HtmlFile);
}

