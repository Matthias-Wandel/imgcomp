//----------------------------------------------------------------------------------
// HTML output for html image viewer for my imgcomp program
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
// Create browsable HTML index for a directory
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
    int ThumbnailHeight;

    Images = Dir->Images;
    Directories = Dir->Dirs;    

	#ifdef _WIN32
		FullresThumbs = 1;
	#endif


    // Header of file.
    printf("<html>\n");
    
    printf("<title>%s</title>\n",Dir->HtmlPath);
    
	ThumbnailHeight = (int)(240/AspectRatio);
	printf(
		"<style type=text/css>\n"
		"  body { font-family: sans-serif; font-size: 24;}\n"
		"  img { vertical-align: middle; margin-bottom: 5px; }\n"
        "  p {margin-bottom: 0px}\n"
        "  div.pix { float:left; width:321px; height:%dpx;}\n", ThumbnailHeight+45);
        
    printf("  div.pix img { width: 320; height: %d;", ThumbnailHeight);
    printf(" margin-bottom:2px; display: block; background-color: #c0c0c0;}\n"
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
            }else{
                printf(" &nbsp &nbsp; &nbsp; &nbsp;");
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
	
		// If there are a LOT of images, don't show all of them
        SkipNum = 0;
		SkipFactor = 1;
		if (num > 8) SkipFactor = 2;
		if (num > 15) SkipFactor = 3;
		if (num > 20) SkipFactor = 4;
		if (num > 40) SkipFactor = 5;
		
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
                printf("<a href=\"view.cgi?%s%s\">",Dir->HtmlPath, Name);
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

//----------------------------------------------------------------------------------
// Create a HTML image view page.
//----------------------------------------------------------------------------------
void MakeImageHtmlOutput(char * ImageName, char * HtmlDir, VarList Images)
{
    int DirIndex;
    int FoundMatch;
    int a;
    int From,To;
    int ShowWidth;
    
    printf("<title>%s</title>\n",ImageName);
    
    printf("<style type=text/css>\n"
           "  body { font-family: sans-serif; font-size: 22;}\n"
           "  img { vertical-align: middle; margin-bottom: 5px; }\n"
           "  p {margin-bottom: 0px}\n"
           "</style></head>\n");

    
    printf("<div style=\"width:950px;\">");
    // Scale it to a resolution that works well on iPad.
    ShowWidth = 950;
    if (ShowWidth/AspectRatio > 535) ShowWidth = (int)535 * AspectRatio;
    printf("<img width=%d height=%d",ShowWidth, (int)(ShowWidth/AspectRatio+0.5));
    printf(" src=\"pix/%s/%s\"></a><br>\n\n",HtmlDir,ImageName);
    
    // Find where the image is in the list of files, or at least what is closest.    
    FoundMatch = DirIndex = 0;
    for (a=0;a<(int)Images.NumEntries;a++){
        int d;
        d = strcmp(ImageName, Images.Entries[a].Name);
        if (d == 0) FoundMatch = 1;
        if (d <= 0){
            DirIndex = a;
            break;
        }
    }

    for (a=0;a<(int)Images.NumEntries;a++){
        int Seconds;
        char * Name = Images.Entries[a].Name;
        Seconds = (Name[7]-'0')*600 + (Name[8]-'0')*60
                + (Name[9]-'0')*10  + (Name[10]-'0');
        Images.Entries[a].DaySecond = Seconds;
    }

    #define FROMTO 5
    From = DirIndex-FROMTO;
    if (From < 0) From = 0;
    To = From+FROMTO*2+1;
    if (To > (int)Images.NumEntries){
        To = Images.NumEntries;
        From = To-FROMTO*2+1;
        if (From < 0) From = 0;
    }
    
    printf("<center>");
    if (From > 0) printf("<< ");
    
    for (a=From;a<To;a++){
        char * NamePtr;
        char TimeStr[10];
        int dt;
        
        // Extract thetime part of the file name to show.
        NamePtr = Images.Entries[a].Name;
        TimeStr[0] = NamePtr[7];
        TimeStr[1] = NamePtr[8];
        TimeStr[2] = ':';
        TimeStr[3] = NamePtr[9];
        TimeStr[4] = NamePtr[10];
        TimeStr[5] = 0;

        if (a > From){
            dt = Images.Entries[a].DaySecond - Images.Entries[a>0?a-1:0].DaySecond;
            if (dt > 1){
                if (dt == 2) printf("&nbsp;");
                if (dt >= 3 && dt < 5) printf("-&nbsp;");
                if (dt >= 5 && dt < 10) printf("--&nbsp;");
                if (dt >= 10 && dt < 20) printf("~~&nbsp;");
                if (dt > 20) printf("||&nbsp;");
            }
        }
        
        if (a == DirIndex && FoundMatch){
            printf("<b>[%s]</b>\n",TimeStr);
        }else{
            if (a == DirIndex) printf("[??]\n");
            //printf(" %d ",dt);
            
            printf("<a href=\"view.cgi?%s%s\">[%s]</a>\n",HtmlDir, NamePtr, TimeStr);
        }
        
    }
    if (To < Images.NumEntries) printf(">>");
    printf("</center>");
    printf("</div>");
    
    printf("<p>\n\n");
    printf("<a href=\"pix/%s/%s\">[Link]</a>\n",HtmlDir,ImageName);
    printf("<a href=\"view.cgi?%s\">[Dir:%s]</a> %d files</a>\n",HtmlDir, HtmlDir, Images.NumEntries);
    
    
    // Show date taken
    // Show settings
}