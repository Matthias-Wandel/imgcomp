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
#else
    #include <sys/stat.h>
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
    int LastSeconds;
    int BreakIndices[61];
    unsigned NumBreakIndices = 0;
    int ThumbnailHeight;
    int DirMinute = 0;
    int AllSameDate;
    int IsKeepDir = 0;
        
    if (strstr(Dir->HtmlPath, "keep") != NULL) IsKeepDir = 1;
    
    Images = Dir->Images;
    Directories = Dir->Dirs;    

    #ifdef _WIN32
        FullresThumbs = 1;
    #endif

    // Header of file.
    printf("<html>\n");
    
    printf("<title>%s</title>\n",Dir->HtmlPath);
    printf("<head><meta charset=\"utf-8\"/>\n");
    
    ThumbnailHeight = (int)(320/AspectRatio);
    printf(
        "<style type=text/css>\n"
        "  body { font-family: sans-serif; font-size: 24;}\n"
        "  img { vertical-align: middle; margin-bottom: 5px; }\n"
        "  p {margin-bottom: 0px}\n"
        "  span {font-family: courier; font-weight: bold; font-size: 18px;}\n"
        "  a {text-decoration: none;}\n"
        "  div.ag { float:left; border-left: 1px solid grey; margin-bottom:10px;}\n"
        "  div.pix { float:left; width:321px; height:%dpx;}\n", ThumbnailHeight+45);
    printf("  div.pix img { width: 320; height: %d;", ThumbnailHeight);
    printf(" margin-bottom:2px; display: block; background-color: #c0c0c0;}\n"
        "</style></head>\n");

    if (strlen(Dir->HtmlPath) > 2){
        if (Dir->Parent[0] == '\0'){
            // Empty query string would mean "today", so make it a '/'
            Dir->Parent[0] = '/';
            Dir->Parent[1] = '\0';
        }
        printf("<a href=\"view.cgi?%s\">[Up:%s]</a>\n",Dir->Parent,Dir->Parent);
    }
    if (Dir->Previous[0]){
        printf("<a href=\"view.cgi?%s\">[Prev:%s]</a>\n",Dir->Previous,Dir->Previous);
    }
    if (Dir->Next[0]){
        printf("<a href=\"view.cgi?%s\">[Next:%s]</a>\n",Dir->Next,Dir->Next);
    }
    
    printf("<p>\n");

    for (b=0;b<Directories.NumEntries;b++){
        char SubdirName[220];
        VarList SubdImages;
        memset(&SubdImages, 0, sizeof(VarList));
        
        printf("<div class='ag'>\n");
        printf("<a href=\"view.cgi?%s/%s\">%s:</a>",Dir->HtmlPath, Directories.Entries[b].Name, Directories.Entries[b].Name);
        
        // Count how many images in subdirectory.
        snprintf(SubdirName,210,"pix/%s/%s",Dir->HtmlPath, Directories.Entries[b].Name);
        CollectDirectory(SubdirName, &SubdImages, NULL, ImageExtensions);
        
        if (SubdImages.NumEntries){
            printf(" <small>(%d)</small>\n",SubdImages.NumEntries);
        }
        printf("<br>\n");
        
        if (!IsKeepDir){
            int Bins[20];
            memset(&Bins, 0, sizeof(Bins));
            for (a=0;a<SubdImages.NumEntries;a++){
                int minute, binno;
                minute = (SubdImages.Entries[a].Name[7]-'0')*10
                    + SubdImages.Entries[a].Name[8]-'0';
                binno = minute/5;
                if (binno >= 0 && binno < 20) Bins[binno] += 1;
            }
        
            printf("<span>");
            for (a=0;a<12;a++){
                if (Bins[a] > 5){
                    char nc = '-';
                    int minute = a*5+2;
                    if (Bins[a] > 12) nc = '1';
                    if (Bins[a] > 40) nc = '2';
                    if (Bins[a] > 100) nc = '#';
                    printf("<a href=\"view.cgi?%s/%s#%02d\">%c</a>",Dir->HtmlPath, Directories.Entries[b].Name, minute, nc);
                }else{
                    printf("&nbsp;");
                }
            }
            printf("</span>\n");
        }
        
        free(SubdImages.Entries);
        
        printf("</div>\n");
        
    }
    
    printf("<br clear=left><p>\n");
    if (Images.NumEntries){
        printf("%s: %d Images<p>\n",Dir->HtmlPath, Images.NumEntries);
    }

    
    {
        // Check if all the images are from the same date.
        char DateStr[10];
        AllSameDate = 1;
        DateStr[0] = 0;
        for (a=0;a<Images.NumEntries;a++){
            char * Name;
            Name = Images.Entries[a].Name;
            if (Name[0] >= '0' && Name[0] <= '9' && Name[1] >= '0' && Name[1] <= '9'){                
                if (AllSameDate){
                    if (DateStr[0] == 0){
                        memcpy(DateStr, Name, 4);
                    }else{
                        if(memcmp(DateStr, Name, 4)){
                            AllSameDate = 0;
                            break;
                        }
                    }
                }
            }
        }
    }
    

    // Find time breaks in images.
    LastSeconds = -1000;
    for (a=0;a<Images.NumEntries;a++){
        int Seconds;
        char * Name;
        Name = Images.Entries[a].Name;
        if (Name[0] >= '0' && Name[0] <= '9' && Name[1] >= '0' && Name[1] <= '9'){
            // Image has numeric name.  Calculate a time value for figuring out gaps and such.
            Seconds = 
                      ((Name[0]-'0') * 10 + (Name[1]-'0'))*3600*24*31 // Month (with gaps for short months)
                    + ((Name[2]-'0') * 10 + (Name[3]-'0'))*3600*24    // Month day
                    + ((Name[5]-'0') * 10 + (Name[6]-'0'))*3600 // Hour
                    + ((Name[7]-'0') * 10 + (Name[8]-'0'))*60   // Minute
                    + ((Name[9]-'0') * 10 + (Name[10]-'0'));    // Seconds
        }else{
            Seconds = 1000000000;
        }
        Images.Entries[a].DaySecond = Seconds;

        if (Seconds-LastSeconds > (AllSameDate ? 60 : 3600)){
            BreakIndices[NumBreakIndices++] = a;
            if (NumBreakIndices > 30) break;
        }
        LastSeconds = Seconds;

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
            char DateStr[10];
            if (!AllSameDate){
                DateStr[0] = Name[0]; DateStr[1] = Name[1];
                DateStr[2] = '-';
                DateStr[3] = Name[2]; DateStr[4] = Name[3];
                DateStr[5] = ' ';
                DateStr[6] = '\0';
            }else{
                DateStr[0] = 0;
            }
            
            
            TimeStr[0] = Name[5]; TimeStr[1] = Name[6];
            TimeStr[2] = ':';
            TimeStr[3] = Name[7]; TimeStr[4] = Name[8];
            TimeStr[5] = ':';
            TimeStr[6] = Name[9]; TimeStr[7] = Name[10];
            TimeStr[8] = '\0';
            printf("<p><big>%s%s</big>\n",DateStr,TimeStr);
        }

        for (a=0;a<num;a++){
            char lc;
            char * Name;
            int dt;
            Name = Images.Entries[a+start].Name;
            lc = Name[strlen(Name)-1];
            if (lc == 'g'){ // It's a jpeg file.
                int Minute;
                if (SkipNum == 0) printf("<div class=\"pix\">\n");

                // Make sure a browser indexable tag exists for every minute.
                Minute = (Name[7]-'0')*10+Name[8]-'0';
                if (Minute > 0 && Minute <= 60 && Minute > DirMinute){
                    while(DirMinute < Minute){
                        printf("<b id=\"%02d\"></b>\n",++DirMinute);
                    }
                }

                printf("<a href=\"view.cgi?%s/%s\">",Dir->HtmlPath, Name);
                if (SkipNum == 0){
                    if (FullresThumbs){
                        printf("<img src=\"pix/%s/%s\">",Dir->HtmlPath, Name);
                    }else{
                        printf("<img src=\"tb.cgi?pix/%s/%s\">",Dir->HtmlPath, Name);
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
                printf("</a>&nbsp;\n");
                SkipNum += 1;
            }else{
                printf("</div><br clear=left><a href=\"pix/%s/%s\">",Dir->HtmlPath, Name);
                printf("%s</a><p>", Name);
                SkipNum = 0;
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
    while(DirMinute < 59){
        printf("<b id=\"%02d\"></b>\n",++DirMinute);
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
    if (!IsKeepDir){
        char KeepDir[20];
        struct stat sb;        
        sprintf(KeepDir, "pix/keep/%.4s",Dir->HtmlPath);
        if (stat(KeepDir, &sb) == 0 && S_ISDIR(sb.st_mode)){
            printf("<a href=\"view.cgi?%s\">[Keep]</a>\n",KeepDir+4);
        }
    }
}

//----------------------------------------------------------------------------------
// Create a HTML to view just one image.
//----------------------------------------------------------------------------------
void MakeImageHtmlOutput(char * ImageName, Dir_t * dir)
{
    int DirIndex;
    int FoundMatch;
    int a;
    int From,To;
    int ShowWidth;
    int ShowHeight;
    int IsKeepDir = 0;
    char * HtmlDir;
    VarList Images;
    Images = dir->Images;
    HtmlDir = dir->HtmlPath;
        
    if (strstr(HtmlDir, "keep/") != NULL) IsKeepDir = 1;
    
    printf("<title>%s</title>\n",ImageName);
    printf("<head><meta charset=\"utf-8\"/>\n");
    
    printf("<style type=text/css>\n"
           "  body { font-family: sans-serif; font-size: 22;}\n"
           "  img { vertical-align: middle; margin-bottom: 5px; }\n"
           "  p {margin-bottom: 0px}\n"
           "  a {text-decoration: none;}\n"
           "</style></head>\n\n");

    // Include a bit of javascript to trigger back-end save without popping up
    // a whole new page that needs to be clicked back from
    printf("<script type=\"text/javascript\">\n"
           "function GetSaveUrl(url){\n"
           "  var xhttp = new XMLHttpRequest()\n"
           "  xhttp.onreadystatechange=function(){\n"
           "    if (this.readyState==4 && this.status==200){\n"
           "      wt=xhttp.responseText.trim()\n"
           "      if(wt.indexOf('Fail:')>=0)\n"
           "        wt=\"<span style='color: rgb(255,0,0);'>[\"+wt+\"]</span>\"\n"
           "      else\n"
           "        wt=\"[\"+wt+\"]\"\n"
           "      document.getElementById(\"save\").innerHTML=wt\n"
           "    }\n"
           "  };\n"
           "  xhttp.open(\"GET\", url, true)\n"
           "  xhttp.send()\n"
           "}\n </script>\n");

    printf("<div style=\"width:950px;\">");
    // Scale it to a resolution that works well on iPad.
    ShowWidth = 950;
    if (ShowWidth/AspectRatio > 535) ShowWidth = (int)535 * AspectRatio;
    ShowHeight = (int)(ShowWidth/AspectRatio+0.5);

    printf("<center>");
    printf("<img width=%d height=%d",ShowWidth, ShowHeight);
    printf(" src=\"pix/%s/%s\" usemap=\"#prevnext\"></a><br>\n\n",HtmlDir,ImageName);
    
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

    // Make left and right parts of the image clickable as previous and next.
    if (Images.Entries){
        printf("<map name=\"prevnext\">\n");
        if (DirIndex > 0 || dir->Previous[0]){
            printf("  <area shape=\"rect\" coords= \"0,0,%d,%d\" ",ShowWidth/4, ShowHeight);
            printf("href=\"view.cgi?%s/%s\">\n", 
                DirIndex > 0 ? HtmlDir : dir->Previous, 
                DirIndex > 0 ? Images.Entries[DirIndex-1].Name : "last.jpg");
        }
        if (DirIndex < Images.NumEntries-1 || dir->Next[0]){
            printf("  <area shape=\"rect\" coords=\"%d,0,%d,%d\" ",(ShowWidth*3)/4, ShowWidth, ShowHeight);
            printf("href=\"view.cgi?%s/%s\">\n",
                DirIndex < Images.NumEntries-1 ? HtmlDir : dir->Next, 
                DirIndex < Images.NumEntries-1 ? Images.Entries[DirIndex+1].Name : "first.jpg");
        }
        printf("</map>\n");
    }

    for (a=0;a<(int)Images.NumEntries;a++){
        int Seconds;
        char * Name = Images.Entries[a].Name;
        Seconds = (Name[7]-'0')*600 + (Name[8]-'0')*60
                + (Name[9]-'0')*10  + (Name[10]-'0');
        Images.Entries[a].DaySecond = Seconds;
    }

    #define BEFORE 4
    #define AFTER  5
    From = DirIndex-BEFORE;
    if (From < 0) From = 0;
    To = From+BEFORE+AFTER+1;
    if (To > (int)Images.NumEntries){
        To = Images.NumEntries;
        From = To-BEFORE-AFTER+1;
        if (From < 0) From = 0;
    }
    
    if (From > 0) printf("<< ");
    
    for (a=From;a<To;a++){
        char TimeStr[10];
        char * NamePtr;
        int dt;
        
        // Extract the time part of the file name to show.
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
            printf("<a href=\"view.cgi?%s/%s\">[%s]</a>\n",HtmlDir, NamePtr, TimeStr);
        }
        
    }
    if (To < Images.NumEntries) printf(">>");
    printf("</center>");
    printf("</div>");
    
    printf("<p>\n\n");
    {
        char IndexInto[8];
        IndexInto[0] = 0;
        if (strlen(ImageName) >= 10){
            IndexInto[0] = '#';
            IndexInto[1] = ImageName[7];
            IndexInto[2] = ImageName[8];
            IndexInto[3] = '\0';
        }
        printf("<a href=\"pix/%s/%s\">[Big]</a>\n",HtmlDir,ImageName);
        printf("<a href=\"tb.cgi?pix/%s/%s$2\">[Adj]</a>\n",HtmlDir,ImageName);
        printf("<a href=\"pix/%s/Log.html#%.2s\">[Log]</a>\n",HtmlDir, ImageName+7);
        
        for (a=0;;a++){
            if (HtmlDir[a] == '/') break;
            if (HtmlDir[a] == '\0'){
                a = 0; break;
            }
        }
        printf("<a href=\"view.cgi?/\">[Dir</a>:");
        printf("<a href=\"view.cgi?%.*s\">", a, HtmlDir);
        printf("%.*s</a>/", a, HtmlDir);
        printf("<a href=\"view.cgi?%s/%s\">%s]</a>\n", HtmlDir, IndexInto, HtmlDir+a+1);

        if (!IsKeepDir){
            char KeepDir[20];
            struct stat sb;
            printf("<a id=\"save\" href=\"#\" onclick=\"GetSaveUrl('view.cgi?~%s/%s\')\">[Save]</a>\n",HtmlDir,ImageName);
            sprintf(KeepDir, "pix/keep/%.4s",HtmlDir);
            if (stat(KeepDir, &sb) == 0 && S_ISDIR(sb.st_mode)){
                printf("<a href=\"view.cgi?%s\">[View saved]</a>\n",KeepDir+4);
            }
        }
    }
}