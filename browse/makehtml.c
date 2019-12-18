//----------------------------------------------------------------------------------
// HTML output for html image viewer for my imgcomp program
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
// Do actagram output
//----------------------------------------------------------------------------------
void ShowActagram(int all, int h24)
{
    int daynum;
    VarList DayDirs;
    memset(&DayDirs, 0, sizeof(DayDirs));

    printf(
        "<style type=text/css>\n"
        "  body { font-family: sans-serif; font-size: 20;}\n"
        "  pre { font-size: 10;}\n"
        "  a {text-decoration: none;}\n"
        "</style>\n");
    
    printf("Actagram: <a href='view.cgi?/'>[Back]</a>"
           "<a href='view.cgi?actagram,all'>[All]</a></big>\n");
    printf("<pre><b>");
    
    CollectDirectory("pix/", NULL, &DayDirs, NULL);

    if (all){
        daynum = 0;
    }else{
        daynum = DayDirs.NumEntries-30;
    }
    
    if (daynum < 0) daynum = 0;
    
    for (;daynum<DayDirs.NumEntries;daynum++){
        int bins[300];
        char DirName[300];
        int a, h;
        VarList HourDirs;
        if (strcmp(DayDirs.Entries[daynum].Name, "keep") == 0) continue;
        
        memset(bins, 0, sizeof(bins));
        
        printf("<a href='view.cgi?%s'>%s</a>",DayDirs.Entries[daynum].Name, DayDirs.Entries[daynum].Name);
        sprintf(DirName, "pix/%s",DayDirs.Entries[daynum].Name);
        memset(&HourDirs, 0, sizeof(HourDirs));
        
        CollectDirectory(DirName, NULL, &HourDirs, NULL);
        
        for (h=0;h<HourDirs.NumEntries;h++){
            int a;
            VarList HourPix;
            memset(&HourPix, 0, sizeof(HourPix));
            sprintf(DirName, "pix/%s/%s",DayDirs.Entries[daynum].Name, HourDirs.Entries[h].Name);
            CollectDirectory(DirName, &HourPix, NULL, ImageExtensions);
            
            for (a=0;a<HourPix.NumEntries;a++){
                int minute, binno;
                char * Name;
                Name = HourPix.Entries[a].Name;
                
                minute = (Name[5]-'0')*60*10 + (Name[6]-'0')*60
                       + (Name[7]-'0')*10    + (Name[8]-'0');
                       
                binno = minute/5;
                if (binno >= 0 && binno < 300) bins[binno] += 1;
            }
            free(HourPix.Entries);
        }
        free (HourDirs.Entries);
        
        // Only show from 6 am to 8:30 pm
        int from = 6*12;
        int to = 12*20+6;
        if (h24){
            from=0;
            to=12*24;
        }
        for (a=from;a<=to;a++){
            char nc = ' ';
            if (a % 12 == 0) nc = '.';
            if (a % 72 == 0) nc = '|';
            
            if (bins[a] > 5) nc = '-';
            if (bins[a] > 12) nc = '1';
            if (bins[a] > 40) nc = '2';
            if (bins[a] > 100) nc = '#';
            
            if (bins[a] > 5){
                char UrlString[100];
                sprintf(UrlString, "view.cgi?%s/%02d/#%02d",DayDirs.Entries[daynum].Name,a/12, a%12*5);
                printf("<a href='%s'>%c</a>",UrlString,nc);
            }else{
                putchar(nc);
            }
        }
        printf("\n");
    }
    free(DayDirs.Entries);
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
    int ThumbnailHeight;
    int DirMinute = 0;
    int AllSameDate;
    int IsKeepDir = 0;
    int IsRoot;
        
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
        "  span {font-family: courier; font-weight: bold; font-size: 14px;}\n"
        "  a {text-decoration: none;}\n"
        "  div.ag { float:left; border-left: 1px solid black; margin-bottom:10px;}\n"
        "  div.pix { float:left; width:321px; height:%dpx;}\n", ThumbnailHeight+45);
    printf("  div.pix img { width: 320; height: %d;", ThumbnailHeight);
    printf(" margin-bottom:2px; display: block; background-color: #c0c0c0;}\n"
        "</style></head>\n");


    printf("%s\n",Dir->HtmlPath);
    if (strlen(Dir->HtmlPath) > 2){
        if (Dir->Parent[0] == '\0'){
            // Empty query string would mean "today", so make it a '/'
            Dir->Parent[0] = '/';
            Dir->Parent[1] = '\0';
        }
        printf("<a href=\"view.cgi?%s\">[Up:%s]</a>\n",Dir->Parent,Dir->Parent);
        IsRoot = 0;
    }else{
        IsRoot = 1;
    }
    if (Dir->Previous[0]){
        printf("<a href=\"view.cgi?%s\">[Prev:%s]</a>\n",Dir->Previous,Dir->Previous);
    }
    if (Dir->Next[0]){
        printf("<a href=\"view.cgi?%s\">[Next:%s]</a>\n",Dir->Next,Dir->Next);
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
        printf("(%4.1f%%) free<p>\n", (double)(sv.f_bavail*100.0/sv.f_blocks));
    }

    for (b=0;b<Directories.NumEntries;b++){
        char SubdirName[220];
        VarList SubdImages;
        
        if (strcmp(Directories.Entries[b].Name, "keep") == 0) continue;
        
        printf("<div class='ag'>\n");
        printf("<a href=\"view.cgi?%s/%s\">%s:</a>",Dir->HtmlPath, Directories.Entries[b].Name, Directories.Entries[b].Name);
        
        // Count how many images in subdirectory.
        snprintf(SubdirName,210,"pix/%s/%s",Dir->HtmlPath, Directories.Entries[b].Name);
        memset(&SubdImages, 0, sizeof(VarList));
        CollectDirectory(SubdirName, &SubdImages, NULL, ImageExtensions);
        
        if (SubdImages.NumEntries){
            printf(" <small>(%d)</small>\n",SubdImages.NumEntries);
        }
        printf("<br>\n");
        
        if (!IsKeepDir){
            int Bins[30];
            memset(&Bins, 0, sizeof(Bins));
            for (a=0;a<SubdImages.NumEntries;a++){
                int minute, binno;
                minute = (SubdImages.Entries[a].Name[7]-'0')*10
                    + SubdImages.Entries[a].Name[8]-'0';
                binno = minute/3;
                if (binno >= 0 && binno < 20) Bins[binno] += 1;
            }
        
            printf("<span>");
            for (a=0;a<20;a++){
                if (Bins[a] > 4){
                    char nc = '-';
                    int minute = a*3+1;
                    if (Bins[a] > 8) nc = '1';
                    if (Bins[a] > 25) nc = '2';
                    if (Bins[a] > 60) nc = '#';
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
    
    if (Directories.NumEntries) printf("<br clear=left><p>\n");
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

        if (Seconds-LastSeconds > (AllSameDate ? 60 : 600)){
            BreakIndices[NumBreakIndices++] = a;
            if (NumBreakIndices > 100) break;
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
            printf("<a href=\"view.cgi?%s\">[Keep]\n",KeepDir+4);
        }
        printf("<a href='/realtime.html'>[Realtime]</a>\n");
        if (strlen(Dir->HtmlPath) < 3){
            printf("<a href='view.cgi?actagram'>[Actagram]</a>\n");
        }
    }
}
