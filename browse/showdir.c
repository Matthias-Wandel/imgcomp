//----------------------------------------------------------------------------------
// HTML based image browser to use with imgcomp output.
//
// HTML output for a list of thumbnails and subdirectories view.
//---------------------------------------------------------------------------------- 
#include <stdio.h>
#include <errno.h>
#include <memory.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <sys/statvfs.h>

#ifdef _WIN32
    #include <io.h>
    #include <direct.h>
#else
    #include <sys/stat.h>
#endif

#include "view.h"

//----------------------------------------------------------------------------------
// Nav linkgs for top and bottom of page.
//----------------------------------------------------------------------------------
static void PrintNavLinks(Dir_t * Dir)
{
    if (strlen(Dir->HtmlPath) > 2){
        printf("<a href=\"view.cgi?%s\">[Up:%s]</a>\n",Dir->Parent,Dir->Parent);
    }
    if (Dir->Previous[0]){
        printf("<a href=\"view.cgi?%s\">[Prev:%s]</a>\n",Dir->Previous,Dir->Previous);
    }
    if (Dir->Next[0]){
        printf("<a href=\"view.cgi?%s\">[Next:%s]</a>\n",Dir->Next,Dir->Next);
    }

    if (!Dir->Dirs.NumEntries){
        printf("<a href=\"view.cgi?%s/\">[JS view]</a>\n",Dir->HtmlPath);
    }
    
    if (strstr(Dir->HtmlPath, "saved") == NULL){
        char SavedDir[50];
        struct stat sb;        
        sprintf(SavedDir, "pix/saved/%.4s",Dir->HtmlPath);
        if (stat(SavedDir, &sb) == 0 && S_ISDIR(sb.st_mode)){
            printf("<a href=\"view.cgi?%s\">[Saved]</a>\n",SavedDir+4);
        }
    }
    printf("<a href='/realtime.html'>[Realtime]</a>\n");
    printf("<a href='view.cgi?actagram'>[Actagram]</a>\n");

}

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
    int BreakIndices[101];
    unsigned NumBreakIndices = 0;
    int DirMinute = 0;
    int AllSameDate;
    int IsSavedDir = 0;
    int IsRoot = 0;
    int HasSubdirImages = 0;
    int ThumbnailHeight = 100;
    float AspectRatio = 0;
        
    if (strstr(Dir->HtmlPath, "saved") != NULL) IsSavedDir = 1;
    
    Images = Dir->Images;
    Directories = Dir->Dirs;    

    #ifdef _WIN32
        FullresThumbs = 1;
    #endif

    // Header of file.
    printf("<html>\n<title>%s</title>\n",Dir->HtmlPath);
    printf("<head><meta charset=\"utf-8\"/>\n");

    // Find first image to determine aspect ratio
    for (a=0;a<Images.NumEntries;a++){
        char * Name = Images.Entries[a].Name;
        int e = strlen(Name);
        if (e < 5 || memcmp(Name+e-4,".jpg",4)) continue; // Not an image.
        char HtmlPath[500];
        sprintf(HtmlPath, "%s/%s", Dir->HtmlPath, Images.Entries[a].Name);
        AspectRatio = ReadExifHeader(HtmlPath, NULL, NULL);
        ThumbnailHeight = (int)(320/AspectRatio);
        break;
    }

    printf(
        "<style type=text/css>\n"
        "  body { font-family: sans-serif; font-size: 24;}\n"
        "  img { vertical-align: middle; margin-bottom: 5px; }\n"
        "  p {margin-bottom: 0px}\n"
        "  span.a {font-family: courier; font-weight: bold; font-size: 14px;}\n"
        "  span.wkend {background-color: #E8E8E8}\n"
        "  a {text-decoration: none;}\n"
        "  div.ag { float:left; border-left: 1px solid black; margin-bottom:10px; min-width:130px;}\n"
        "  div.pix { float:left; width:321px; height:%dpx;}\n", ThumbnailHeight+45);
    printf("  div.pix img { width: 320; height: %d;", ThumbnailHeight);
    printf(" margin-bottom:2px; display: block; background-color: #c0c0c0;}\n"
        "</style></head>\n");

    //printf("%s\n",Dir->HtmlPath);
    if (strlen(Dir->HtmlPath) > 2){
        if (Dir->Parent[0] == '\0'){
            // Empty query string would mean "today", so make it a '/'
            Dir->Parent[0] = '/';
            Dir->Parent[1] = '\0';
        }
    }else{
        IsRoot = 1;
    }

    
    if (!Directories.NumEntries){
        printf("<a href=\"view.cgi?%s/\">[JS view]</a>\n",Dir->HtmlPath);
    }

    
    printf("<p>\n");
        
    if (IsRoot){
        struct statvfs sv;
        char HostName[30];
        FILE * fp;
        fp = fopen("/proc/sys/kernel/hostname","r");
        memset(HostName, 0, 30);
        fread(HostName, 1, 30, fp);
        fclose(fp);
        
        statvfs("pix/", &sv);
        printf("%s: %5.1f gig ", HostName, (double)sv.f_bavail * sv.f_bsize / 1000000000);
        printf("(%4.1f%%) free<br>\n", (double)(sv.f_bavail*100.0/sv.f_blocks));
    }
    
    PrintNavLinks(Dir);
    puts("<br>");

    int prevwkd = 6, thiswkd=6;

    for (b=0;b<Directories.NumEntries;b++){
        VarList SubdImages;
        char * SubdirName = Directories.Entries[b].Name;
        
        if (strcmp(SubdirName, "saved") == 0) continue;
        
        int isw = -1;
        if (IsRoot){
            isw = IsWeekendString(SubdirName);
            thiswkd = isw & 7;
            if (thiswkd <= prevwkd){
                printf("<br clear=left>");
            }
            prevwkd = thiswkd;
        }
        
        printf("<div class='ag'>\n");
        if (isw >= 0) printf("<span class=\"wkend\">");
        printf("<a href=\"view.cgi?%s/%s\">%s:</a>",Dir->HtmlPath, SubdirName, Directories.Entries[b].Name);
        if (isw >= 0) printf("</span>");
        
        // Count how many images in subdirectory.
        {
            char SubdirPath[220];
            snprintf(SubdirPath,210,"pix/%s/%s",Dir->HtmlPath, SubdirName);
            memset(&SubdImages, 0, sizeof(VarList));
            CollectDirectory(SubdirPath, &SubdImages, NULL, ImageExtensions);
        }
        
        if (SubdImages.NumEntries){
            printf(" <small>(%d)</small>\n",SubdImages.NumEntries);
            HasSubdirImages = 1;
        }
        printf("<br>\n");
        
        if (!IsSavedDir && !IsRoot){
            // Build an actagram for the hour.
            const int NumBins = 30; // Bins per hour.
            int Bins[NumBins];
            int BinImage[NumBins];
            memset(&Bins, 0, sizeof(Bins));
            for (a=0;a<SubdImages.NumEntries;a++){
                int Second, binno;
                char * Name = SubdImages.Entries[a].Name;
                int e = strlen(Name);
                if (e < 5 || memcmp(Name+e-4,".jpg",4)) continue; // Not an image.

                Second = (Name[7]-'0')*600 + (Name[8]-'0') * 60
                        +(Name[9]-'0')*10  + (Name[19]-'0');
                binno = Second*NumBins/3600;
                if (binno >= 0 && binno < NumBins){
                    Bins[binno] += 1;
                    BinImage[binno] = a-Bins[binno]/2;
                }

                if (AspectRatio == 0){
                    char ImgPath[500];
                    sprintf(ImgPath, "%s/%s/%s", Dir->HtmlPath, SubdirName, Name);
                    AspectRatio = ReadExifHeader(ImgPath, NULL, NULL);
                }
            }
        
            printf("<span class='a'>");
            for (a=0;a<NumBins;a++){
                if (Bins[a]){
                    char nc = '-';
                    if (Bins[a] >= 1) nc = '.';
                    if (Bins[a] >= 8) nc = '1';
                    if (Bins[a] >= 25) nc = '2';
                    if (Bins[a] >= 60) nc = '#';
                    char * Name = SubdImages.Entries[BinImage[a]].Name;
                    printf("<a href=\"view.cgi?%s/%s/#%.5s\"",Dir->HtmlPath, SubdirName, Name+7);
                    printf(" onmouseover=\"mmo('%s/%s')\"",SubdirName, Name);
                    printf(">%c</a>", nc);
                }else{
                    printf("&nbsp;");
                }
            }
            printf("</span.a>\n");
        }
        free(SubdImages.Entries);
        printf("</div>\n");
    }

    if (Directories.NumEntries) printf("<br clear=left><p>\n");

    int NumImages = 0;
    
    // Check if all the images are from the same date.
    char DateStr[10];
    AllSameDate = 1;
    DateStr[0] = 0;
    for (a=0;a<Images.NumEntries;a++){
        char * Name = Images.Entries[a].Name;
        int e = strlen(Name);
        if (e < 5 || memcmp(Name+e-4,".jpg",4)) continue; // Not an image.
        NumImages += 1;

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

    if (Images.NumEntries){
        printf("Directory <b>%s</b>: <b>%d</b> Images<p>\n",Dir->HtmlPath, NumImages);
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

        if (Seconds-LastSeconds > (AllSameDate ? 60 : 600)){
            BreakIndices[NumBreakIndices++] = a;
            if (NumBreakIndices > 100) break;
        }
        LastSeconds = Seconds;

    }
    BreakIndices[NumBreakIndices] = Images.NumEntries;

    // Show continuous runs of images, with breaks between.
    for (b=0;b<NumBreakIndices;b++){
        int start = BreakIndices[b];
        int num = BreakIndices[b+1]-BreakIndices[b];

        // If there are a LOT of images, don't show all of them
        int SkipNum = 0;
        int SkipFactor = 1;
        if (num > 8) SkipFactor = 2;
        if (num > 15) SkipFactor = 3;
        if (num > 20) SkipFactor = 4;
        if (num > 40) SkipFactor = 5;

        char * Name = Images.Entries[start].Name;
        
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
            char TimeStr[10];
            TimeStr[0] = Name[5]; TimeStr[1] = Name[6];
            TimeStr[2] = ':';
            TimeStr[3] = Name[7]; TimeStr[4] = Name[8];
            TimeStr[5] = ':';
            TimeStr[6] = Name[9]; TimeStr[7] = Name[10];
            TimeStr[8] = '\0';
            printf("<p><big>%s%s</big>\n",DateStr,TimeStr);
        }

        for (a=0;a<num;a++){
            char * Name = Images.Entries[a+start].Name;
            int e = strlen(Name);
            if (e >= 5 && memcmp(Name+e-4,".jpg",4) == 0){// It's a jpeg file.
                int Minute;
                if (SkipNum == 0) printf("<div class=\"pix\">\n");

                // Make sure a browser indexable tag exists for every minute.
                Minute = (Name[7]-'0')*10+Name[8]-'0';
                if (Minute > 0 && Minute <= 60 && Minute > DirMinute){
                    while(DirMinute < Minute){
                        printf("<b id=\"%02d\"></b>\n",++DirMinute);
                    }
                }
                //printf("<a href=\"view.cgi?%s/%s\">",Dir->HtmlPath, Name);
                printf("<a href=\"view.cgi?%s/#%.5s\">",Dir->HtmlPath, Name+7);
                if (SkipNum == 0){
                    if (FullresThumbs){
                        printf("<img src=\"pix/%s/%s\">",Dir->HtmlPath, Name);
                    }else{
                        printf("<img src=\"tb.cgi?%s/%s\">",Dir->HtmlPath, Name);
                    }
                    if (num > 1){
                        char TimeStr[10];
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
            int dt = 0;
            if (a < num-1) dt = Images.Entries[a+1+start].DaySecond - Images.Entries[a+start].DaySecond;
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
    PrintNavLinks(Dir);

    // Add javascript for hover-over preview when showing a whole day's worth of images
    if (HasSubdirImages){
        printf("<p><small id='prevn'></small><br>\n"
               "<a id='prevh' href=''><img id='preview' src='' width=0 height=0></a>\n");

        // Javascript
        printf("<script>\n"
               "function mmo(str){\n"
               "el = document.getElementById('preview')\n");
        printf("   el.src = '/pix/%s/'+str\n",Dir->HtmlPath);
        printf("   el.width = 800\n"
               "   el.height = %d\n",(int)(800/AspectRatio));
        printf("el = document.getElementById('prevh')\n"
               "   el.href = '/view.cgi?/%s/'+str\n",Dir->HtmlPath);
        printf("el = document.getElementById('prevn')\n"
               "   el.innerHTML = str + ' &nbsp; &nbsp;'"
               "   +str.substring(8, 10)+':'+str.substring(10,12);");
        printf("}\n"
               "</script>\n");
    }
}
