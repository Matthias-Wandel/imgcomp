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
// Determine if it's a weekend day or a holiday
// Returns < 0 for weekday, >= 0 for weekend/holiday.  3 lsbs indicate day of week.
//----------------------------------------------------------------------------------
static int IsWeekendString(char * DirString)
{
    int a, daynum, weekday;
    int y,m,d;
    daynum=0;
    for (a=0;a<6;a++){
        if (DirString[a] < '0' || DirString[a] > '9') break;
        daynum = daynum*10+DirString[a]-'0';
    }
    if (a != 6 || DirString[6] != '\0'){
        // dir name must be six digits only.

        return -10;
    }

    d = daynum%100;
    m = (daynum/100)%100;
    y = daynum/10000 + 2000;
    //printf("%d %d %d",d,m,y);

    {
        // A clever bit of code from the internet!
        static int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
        y -= m < 3;
        weekday = ( y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
        // Sunday = 0, Saturday = 6
    }

    // New brunswick holidays thru end of 2021.
    static const int Holidays[] = {190701, 190902, 191225,
                200101, 200217, 200410, 200518, 200701, 200907, 201225,
                210101, 210402, 210524, 210701, 210802, 210906, 211227};

    for (a=0;a<sizeof(Holidays)/sizeof(int);a++){
        // Check if date is a holday.
        if (Holidays[a] == daynum) return weekday;
    }

    if (weekday == 0 || weekday == 6) return weekday;

    return weekday-8;
}

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
        "  span.wkend {background-color: #E8E8E8}\n"
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
        daynum = DayDirs.NumEntries-45;
    }
    
    if (daynum < 0) daynum = 0;

    int prevwkd = 6, thiswkd=6;
    
    for (;daynum<DayDirs.NumEntries;daynum++){
        int bins[300];
        char DirName[300];
        int a, h;
        VarList HourDirs;
        if (strcmp(DayDirs.Entries[daynum].Name, "keep") == 0) continue;
        
        memset(bins, 0, sizeof(bins));

        int isw = IsWeekendString(DayDirs.Entries[daynum].Name);

        thiswkd = isw & 7;
        if (thiswkd != (prevwkd+1) %7){
            printf("<br>");
        }
        prevwkd = thiswkd;


        if (isw >= 0) printf("<span class=\"wkend\">");
        
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
        if (isw >= 0) printf("</span>");
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
    int DirMinute = 0;
    int AllSameDate;
    int IsKeepDir = 0;
    int IsRoot;
    int HasSubdirImages = 0;
    int ThumbnailHeight = 100;
    float AspectRatio = 0;
        
    if (strstr(Dir->HtmlPath, "keep") != NULL) IsKeepDir = 1;
    
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
        AspectRatio = ReadExifHeader(HtmlPath);
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

    int prevwkd = 6, thiswkd=6;

    for (b=0;b<Directories.NumEntries;b++){
        VarList SubdImages;
        char * SubdirName = Directories.Entries[b].Name;
        
        if (strcmp(SubdirName, "keep") == 0) continue;
        
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
        
        if (!IsKeepDir){
            int Bins[30];
            int BinImage[30];
            memset(&Bins, 0, sizeof(Bins));
            for (a=0;a<SubdImages.NumEntries;a++){
                int minute, binno;
                char * Name = SubdImages.Entries[a].Name;
                int e = strlen(Name);
                if (e < 5 || memcmp(Name+e-4,".jpg",4)) continue; // Not an image.

                minute = (Name[7]-'0')*10 + Name[8]-'0';
                binno = minute/3;
                if (binno >= 0 && binno < 20){
                    Bins[binno] += 1;
                    BinImage[binno] = a-Bins[binno]/2;
                }

                if (AspectRatio == 0){
                    char ImgPath[500];
                    sprintf(ImgPath, "%s/%s/%s", Dir->HtmlPath, SubdirName, Name);
                    AspectRatio = ReadExifHeader(ImgPath);
                }
            }
        
            printf("<span class='a'>");
            for (a=0;a<20;a++){
                if (Bins[a]){
                    char nc = '-';
                    int minute = a*3+1;
                    if (Bins[a] >= 1) nc = '.';
                    if (Bins[a] >= 8) nc = '1';
                    if (Bins[a] >= 25) nc = '2';
                    if (Bins[a] >= 60) nc = '#';
                    printf("<a href=\"view.cgi?%s/%s#%02d\"",Dir->HtmlPath, SubdirName, minute);
                    printf(" onmouseover=\"mmo('%s/%s')\"",SubdirName, SubdImages.Entries[BinImage[a]].Name);
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
    
    {
        // Check if all the images are from the same date.
        char DateStr[10];
        AllSameDate = 1;
        DateStr[0] = 0;
        for (a=0;a<Images.NumEntries;a++){
            char * Name;
            Name = Images.Entries[a].Name;
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
    }

    if (Images.NumEntries){
        printf("%s: %d Images<p>\n",Dir->HtmlPath, NumImages);
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
    }
    printf("<a href='/realtime.html'>[Realtime]</a>\n");
    printf("<a href='view.cgi?actagram'>[Actagram]</a><p>\n");

    // Add javascript for hover-over preview when showing a whole day's worth of images
    if (HasSubdirImages){
        printf("<script>\n"
           "function mmo(str) {\n"
           "   document.getElementById(\"demo\").innerHTML = str;\n"
           "}\n"
           "</script>\n");

        //printf("Demo:<b id='demo'>foo</b><p>");
        printf("<img id='preview' src='' width=0 height=0>\n");

        // Javascript
        printf("<script>\n"
           "function mmo(str){\n"
           "el = document.getElementById('preview')\n");
        //printf("   document.getElementById('demo').innerHTML = '/pix/%s/'+str\n",Dir->HtmlPath);
        printf("   el.src = '/pix/%s/'+str\n",Dir->HtmlPath);
        printf("   el.width = 800\n"
           "   el.height = %d\n",(int)(800/AspectRatio));
        printf("}\n"
               "</script>\n");
    }
}
