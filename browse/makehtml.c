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
    unsigned a, b;
	int SkipFactor;
	int FullresThumbs = 0;
	int LastDaySeconds;
	int BreakIndices[61];
	unsigned NumBreakIndices = 0;

    Images = Dir->Images;
    Directories = Dir->Dirs;    

	#ifdef _WIN32
		FullresThumbs = 1;
	#endif

	HtmlFile = stdout;

    // Header of file.
    fprintf(HtmlFile, "<html>\n");
	
	fprintf(HtmlFile, 
		"<style type=text/css>\n"
		"  body { font-family: sans-serif; font-size: 24;}\n"
		"  img { vertical-align: middle; margin-bottom: 5px; }\n"
		"</style></head>\n");
		
	if (strlen(Dir->HtmlPath) > 2){
		fprintf(HtmlFile,"<a href=\"view.cgi?%s\">[Up:%s]</a>\n",Dir->Parent,Dir->Parent);
	}
	if (Dir->Previous[0]){
		fprintf(HtmlFile,"<a href=\"view.cgi?%s\">[Prev:%s]</a>\n",Dir->Previous,Dir->Previous);
	}
	if (Dir->Next[0]){
		fprintf(HtmlFile,"<a href=\"view.cgi?%s\">[Next:%s]</a>\n",Dir->Next,Dir->Next);
	}
    fprintf(HtmlFile, "<p>\n");

    for (a=0;a<Directories.NumEntries;a++){

        //VarList Dirs;
        
        
		fprintf(HtmlFile, "<a href=\"view.cgi?%s%s\">%s</a>",Dir->HtmlPath, Directories.Entries[a].Name, Directories.Entries[a].Name);
        
        // Count how many images in subdirectory.
        {
            char SubdirName[200];
            VarList SubdImages;
            memset(&SubdImages, 0, sizeof(VarList));
            snprintf(SubdirName,200,"pix/%s%s",Dir->HtmlPath, Directories.Entries[a].Name);
            CollectDirectory(SubdirName, &SubdImages, NULL, ImageExtensions);
            if (SubdImages.NumEntries){
                fprintf(HtmlFile, "(%d)\n",SubdImages.NumEntries);
                if (SubdImages.NumEntries < 100) fprintf(HtmlFile, " &nbsp");
                if (SubdImages.NumEntries < 10) fprintf(HtmlFile, " &nbsp");
            }
            free(SubdImages.Entries);
        }
        
        
		if ((a % 5 ) == 4){
			fprintf(HtmlFile, "<p>\n"); // Put four links per line.
		}else{
			fprintf(HtmlFile, " &nbsp; \n"); // Put four links per line.
		}
    }
	
	fprintf(HtmlFile,"<p>\n");
	if (Images.NumEntries){
		fprintf(HtmlFile,"%s: %d Images<p>\n",Dir->HtmlPath, Images.NumEntries);
	}
	
	LastDaySeconds = -1000;
	for (a=0;a<Images.NumEntries;a++){
		int DaySeconds;
		char * Name;
		Name = Images.Entries[a].Name;
		DaySeconds = ((Name[5]-'0') * 10 + (Name[6]-'0'))*3600
	               + ((Name[7]-'0') * 10 + (Name[8]-'0'))*60
				   + ((Name[9]-'0') * 10 + (Name[10]-'0'));
		
		if (DaySeconds-LastDaySeconds > 60){
			BreakIndices[NumBreakIndices++] = a;
			printf("\n");
			if (NumBreakIndices > 60) break;
		}
		LastDaySeconds = DaySeconds;
				   
	}
	BreakIndices[NumBreakIndices] = Images.NumEntries;
	
	
	for (b=0;b<NumBreakIndices;b++){
		unsigned start, num;
		char TimeStr[10];
		char * Name;
		start = BreakIndices[b];
		num = BreakIndices[b+1]-BreakIndices[b];
	
		// If there are a LOT of images, don't show all of them!
		SkipFactor = 1;
		if (num > 10) SkipFactor = 2;
		if (num > 20) SkipFactor = 3;
		if (num > 30) SkipFactor = 4;
		if (num > 60) SkipFactor = 5;
		
		Name = Images.Entries[start].Name;
		TimeStr[0] = Name[5];
		TimeStr[1] = Name[6];
		TimeStr[2] = ':';
		TimeStr[3] = Name[7];
		TimeStr[4] = Name[8];
		TimeStr[5] = ':';
		TimeStr[6] = Name[9];
		TimeStr[7] = Name[10];
		TimeStr[8] = '\0';
		printf("<p>%s<br>",TimeStr);
	
		for (a=0;a<num;a++){
			fprintf(HtmlFile, "<a href=\"pix/%s%s\">",Dir->HtmlPath, Images.Entries[a+start].Name);
			if (a % SkipFactor == 0){
				if (FullresThumbs){
					fprintf(HtmlFile, "<img src=\"pix/%s%s\"",Dir->HtmlPath, Images.Entries[a+start].Name);
				}else{
					fprintf(HtmlFile, "<img src=\"tb.cgi?pix/%s%s\"",Dir->HtmlPath, Images.Entries[a+start].Name);
				}
				fprintf(HtmlFile, " width=320 height=240>");
			}else{
				fprintf(HtmlFile, "<big><big>#</big></big>");
			}
			fprintf(HtmlFile, "</a>\n");
		}
	}
	
    fprintf(HtmlFile, "<p>\n");    
	if (strlen(Dir->HtmlPath) > 2){
		fprintf(HtmlFile,"<a href=\"view.cgi?%s\">[Up:%s]</a>\n",Dir->Parent,Dir->Parent);
	}
	if (Dir->Previous[0]){
		fprintf(HtmlFile,"<a href=\"view.cgi?%s\">[Prev:%s]</a>\n",Dir->Previous,Dir->Previous);
	}
	if (Dir->Next[0]){
		fprintf(HtmlFile,"<a href=\"view.cgi?%s\">[Next:%s]</a>\n",Dir->Next,Dir->Next);
	}


	if (HtmlFile != stdout) fclose(HtmlFile);
}

