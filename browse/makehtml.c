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

    unsigned a, b;
	int FullresThumbs = 0;
	int LastDaySeconds;
	int BreakIndices[61];
	unsigned NumBreakIndices = 0;

    Images = Dir->Images;
    Directories = Dir->Dirs;    

	#ifdef _WIN32
		FullresThumbs = 1;
	#endif


    // Header of file.
    printf("<html>\n");
	
	printf(
		"<style type=text/css>\n"
		"  body { font-family: sans-serif; font-size: 24;}\n"
		"  img { vertical-align: middle; margin-bottom: 5px; }\n"
        "  p {margin-bottom: 0px}\n"
        "  div.pix { float:left; width:328px; height:285px;}\n"
        "  div.pix img { width: 320; height: 240; margin-bottom:2px; display: block; background-color: #c0c0c0;}\n"
		"</style></head>\n");
		
	if (strlen(Dir->HtmlPath) > 2){
		printf("<a href=\"view.cgi?%s\">[Up:%s]</a>\n",Dir->Parent,Dir->Parent);
	}
	if (Dir->Previous[0]){
		printf("<a href=\"view.cgi?%s\">[Prev:%s]</a>\n",Dir->Previous,Dir->Previous);
	}
	if (Dir->Next[0]){
		printf("<a href=\"view.cgi?%s\">[Next:%s]</a>\n",Dir->Next,Dir->Next);
	}
    printf("<p>\n");

    for (a=0;a<Directories.NumEntries;a++){
        
		printf("<a href=\"view.cgi?%s%s\">%s</a>",Dir->HtmlPath, Directories.Entries[a].Name, Directories.Entries[a].Name);
        
        // Count how many images in subdirectory.
        {
            char SubdirName[220];
            VarList SubdImages;
            memset(&SubdImages, 0, sizeof(VarList));
            snprintf(SubdirName,210,"pix/%s%s",Dir->HtmlPath, Directories.Entries[a].Name);
            CollectDirectory(SubdirName, &SubdImages, NULL, ImageExtensions);
            if (SubdImages.NumEntries){
                printf("(%d)\n",SubdImages.NumEntries);
                if (SubdImages.NumEntries < 100) printf(" &nbsp");
                if (SubdImages.NumEntries < 10) printf(" &nbsp");
            }
            free(SubdImages.Entries);
        }
        
        
		if ((a % 5 ) == 4){
			printf("<p>\n"); // Put four links per line.
		}else{
			printf(" &nbsp; \n"); // Put four links per line.
		}
    }
	
	printf("<p>\n");
	if (Images.NumEntries){
		printf("%s: %d Images<p>\n",Dir->HtmlPath, Images.NumEntries);
	}
	
    // Find time breaks in images.
	LastDaySeconds = -1000;
	for (a=0;a<Images.NumEntries;a++){
		int DaySeconds;
		char * Name;
		Name = Images.Entries[a].Name;
        if (Name[0] >= '0' && Name[0] <= '9' && Name[1] >= '0' && Name[1] <= '9'){
            // Numeric name.
            DaySeconds = ((Name[5]-'0') * 10 + (Name[6]-'0'))*3600
                    + ((Name[7]-'0') * 10 + (Name[8]-'0'))*60
                    + ((Name[9]-'0') * 10 + (Name[10]-'0'));
        }else{
            DaySeconds = 1000000;
        }
        Images.Entries[a].DaySecond = DaySeconds;
		
		if (DaySeconds-LastDaySeconds > 60){
			BreakIndices[NumBreakIndices++] = a;
			printf("\n");
			if (NumBreakIndices > 30) break;
		}
		LastDaySeconds = DaySeconds;
				   
	}
	BreakIndices[NumBreakIndices] = Images.NumEntries;
	
	
    // Show continuous runs of images, with breaks between.
	for (b=0;b<NumBreakIndices;b++){
		unsigned start, num;
		char TimeStr[10];
		char * Name;
        int SkipFactor, SkipNum;
		start = BreakIndices[b];
		num = BreakIndices[b+1]-BreakIndices[b];
	
		// If there are a LOT of images, don't show all of them!
        SkipNum = 0;
		SkipFactor = 1;
		if (num > 10) SkipFactor = 2;
		if (num > 20) SkipFactor = 3;
		if (num > 30) SkipFactor = 4;
		if (num > 60) SkipFactor = 5;
		
		Name = Images.Entries[start].Name;
        if (Name[0] >= '0' && Name[0] <= '9' && Name[1] >= '0' && Name[1] <= '9'){
            TimeStr[0] = Name[5]; TimeStr[1] = Name[6];
            TimeStr[2] = ':';
            TimeStr[3] = Name[7]; TimeStr[4] = Name[8];
            TimeStr[5] = ':';
            TimeStr[6] = Name[9]; TimeStr[7] = Name[10];
            TimeStr[8] = '\0';
            printf("<p><big>%s</big>\n",TimeStr);
        }
	
		for (a=0;a<num;a++){
            char lc;
            char * Name;
            int dt;
            Name = Images.Entries[a+start].Name;
            lc = Name[strlen(Name)-1];
            if (lc == 'g'){ // It's a jpeg file.
                if (SkipNum == 0) printf("<div class=\"pix\">\n");
                printf("<a href=\"pix/%s%s\">",Dir->HtmlPath, Name);
                if (SkipNum == 0){
                    if (FullresThumbs){
                        printf("<img src=\"pix/%s%s\">",Dir->HtmlPath, Name);
                    }else{
                        printf("<img src=\"tb.cgi?pix/%s%s\">",Dir->HtmlPath, Name);
                    }
                    if (num > 1){
                        TimeStr[0] = Name[5]; TimeStr[1] = Name[6];
                        TimeStr[2] = ':';
                        TimeStr[3] = Name[7]; TimeStr[4] = Name[8];
                        TimeStr[5] = ':';
                        TimeStr[6] = Name[9]; TimeStr[7] = Name[10];
                        TimeStr[8] = '\0';
                        printf("%s</a>\n",TimeStr);
                    }
                }else{
                    printf(":%c%c", Name[9], Name[10]);
                }
                printf("</a> &nbsp;\n");
                SkipNum += 1;
            }else{
                printf("<p><a href=\"pix/%s%s\">",Dir->HtmlPath, Name);
                printf("<p>%s<p>", Name);
            }
            dt = 0;
            if (a < num-1) dt = Images.Entries[a+1+start].DaySecond - Images.Entries[a+start].DaySecond;
            //printf("(%d)",dt);
            if (SkipNum >= SkipFactor || a >= num-1 || dt > 3){
                if (dt <= 3 && a < num-1) printf("...");
                printf("</div>\n");
                SkipNum = 0;
            }
		}
        printf("<br clear=left>\n");
	}
	
    printf("<p>\n");    
	if (strlen(Dir->HtmlPath) > 2){
		printf("<a href=\"view.cgi?%s\">[Up:%s]</a>\n",Dir->Parent,Dir->Parent);
	}
	if (Dir->Previous[0]){
		printf("<a href=\"view.cgi?%s\">[Prev:%s]</a>\n",Dir->Previous,Dir->Previous);
	}
	if (Dir->Next[0]){
		printf("<a href=\"view.cgi?%s\">[Next:%s]</a>\n",Dir->Next,Dir->Next);
	}
}

