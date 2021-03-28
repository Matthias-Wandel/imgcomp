//----------------------------------------------------------------------------------
// HTML based image browser to use with imgcomp output.
//
// HTML output for a list of thumbnails and subdirectories view.
//
// Imgcomp and html browsing tool is licensed under GPL v2 (see README.txt)
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
static void PrintNavLinks(Dir_t * Dir, int IsRoot)
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
        if (IsRoot){
            strcpy(SavedDir, "pix/saved");
        }else{
            sprintf(SavedDir, "pix/saved/%.4s",Dir->HtmlPath);
        }
        
        if (stat(SavedDir, &sb) == 0 && S_ISDIR(sb.st_mode)){
            printf("<a href=\"view.cgi?%s\">[Saved]</a>\n",SavedDir+4);
        }
    }
    printf("<a href='realtime.html'>[Realtime]</a>\n");
    printf("<a href='view.cgi?actagram'>[Actagram]</a>\n");
}

//----------------------------------------------------------------------------------
// Show an actagram for one hour.
//----------------------------------------------------------------------------------
static void ShowHourActagram(VarList SubdImages, char * HtmlPath, char * SubdirName)
{
    // Build an actagram for the hour.
    const int MaxBins = 60; // Bins per hour.
    int Bins[MaxBins];
    int BinImage[MaxBins];
    int NumBins = MaxBins;
    int NumImages = 0;
    memset(&Bins, 0, sizeof(Bins));
    for (int a=0;a<SubdImages.NumEntries;a++){
        int Second, binno;
        char * Name = SubdImages.Entries[a].Name;
        int e = strlen(Name);
        if (e < 5 || memcmp(Name+e-4,".jpg",4)) continue; // Not an image.
        NumImages += 1;

        Second = (Name[7]-'0')*600 + (Name[8]-'0') * 60
                +(Name[9]-'0')*10  + (Name[10]-'0');
        binno = Second*NumBins/3600;
        if (binno >= 0 && binno < NumBins){
            Bins[binno] += 1;
            BinImage[binno] = a-Bins[binno]/2;
        }
    }

    int hrefOpen = 0;
    printf("<ul class='histo'>\n");
    for (int a=0;a<NumBins;a++){
        if (Bins[a]){
            if (hrefOpen) printf("</a>");
            char * Name = SubdImages.Entries[BinImage[a]].Name;
            printf("<li>\n<a href=\"view.cgi?%s/%s/#%s\"",HtmlPath, SubdirName, Name);
            printf(" onmouseover=\"mmo('%s/%s')\">",SubdirName, Name);
            printf("<span class=\"height\" style=\"height: %d%%;\">(%d)</span>\n</li>\n", (Bins[a]*28+5)/6, Bins[a]);
            hrefOpen = 1;
        }else{
            printf("<li></li>");
            if (hrefOpen) printf("</a>");
            hrefOpen = 0;
            printf("\n");
        }
    }
    if (hrefOpen) printf("</a>");
    printf("</ul>\n");
}

//----------------------------------------------------------------------------------
// Show directories for all the hours (or days)
//----------------------------------------------------------------------------------
static int ShowHourlyDirs(char * HtmlPath, int IsRoot, VarList Directories)
{
    int TotImages = 0;

    int prevwkd = 6, thiswkd=6;
    for (int b=0;b<Directories.NumEntries;b++){
        char * SubdirName = Directories.Entries[b].Name;
        
        if (strcmp(SubdirName, "saved") == 0) continue;
        
        int isw = IsWeekendString(SubdirName);
        thiswkd = isw & 7;
        if (thiswkd < prevwkd){
            printf("<br clear=left>");
        }
        prevwkd = thiswkd;

        VarList SubdImages;
        char SubdirPath[220];
        memset(&SubdImages, 0, sizeof(VarList));
        snprintf(SubdirPath,210,"pix/%s/%s",HtmlPath, SubdirName);
        CollectDirectory(SubdirPath, &SubdImages, NULL, ImageExtensions);

        printf("<div class='ag'>\n");
        if (isw >= 0) printf("<span class=\"wkend\">");
        printf("<a href=\"view.cgi?%s/%s",HtmlPath, SubdirName);
        if (SubdImages.NumEntries) printf("/");
        printf("\">%s:</a>\n",Directories.Entries[b].Name);
        if (isw >= 0) printf("</span>");

        if (SubdImages.NumEntries){
            printf("<small><small>(<a href=\"view.cgi?%s/%s\">",HtmlPath, SubdirName);
            printf("%d</a>)</small></small>\n",SubdImages.NumEntries);
            TotImages += SubdImages.NumEntries;
        }
        printf("<br>\n");

        if (!IsRoot) {
            ShowHourActagram(SubdImages, HtmlPath, SubdirName);
        }
        free(SubdImages.Entries);

        printf("</div>\n");
    }
    printf("<br clear=left>\n");
    if (TotImages) printf("<br>%d images<br>",TotImages);
    
    return TotImages;
}

//----------------------------------------------------------------------------------
// Show list of thumbnails in this directory.
//----------------------------------------------------------------------------------
static void ShowThumbnailList(char * HtmlPath, int IsSavedDir, VarList Images)
{
    int NumImages = 0;
    
    // Check if all the images are from the same date.
    char DateStr[10];
    int AllSameDate = 1;
    DateStr[0] = 0;
    for (int a=0;a<Images.NumEntries;a++){
        char * Name = Images.Entries[a].Name;
        int e = strlen(Name);
        if (e < 5 || memcmp(Name+e-4,".jpg",4)) continue; // Not an image.
        NumImages += 1;
    
        if (AllSameDate){
            if (Name[0] >= '0' && Name[0] <= '9' && Name[1] >= '0' && Name[1] <= '9'){
                if (DateStr[0] == 0){
                    memcpy(DateStr, Name, 4);
                }else{
                    if(memcmp(DateStr, Name, 4)){
                        AllSameDate = 0;
                    }
                }
            }
        }
    }
    
    printf("Directory <b>%s</b>: <b>%d</b> Images<p>\n",HtmlPath, NumImages);

    // Find time breaks in images.
    int LastSeconds = -1000;
    int BreakIndices[101];
    int NumBreakIndices = 0;
    
    for (int a=0;a<Images.NumEntries;a++){
        char * Name = Images.Entries[a].Name;
        
        int Seconds;
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
    
    int DirMinute = 0;
    
    // Show continuous runs of images, with breaks between.
    for (int b=0;b<NumBreakIndices;b++){
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
            if (!AllSameDate || IsSavedDir){
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
    
        for (int a=0;a<num;a++){
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
                printf("<a href=\"view.cgi?%s/#%s\">",HtmlPath, Name);

                if (SkipNum == 0){
                    printf("<img src=\"tb.cgi?%s/%s\">",HtmlPath, Name);
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
                printf("</div><br clear=left><a href=\"pix/%s/%s\">",HtmlPath, Name);
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
}

//----------------------------------------------------------------------------------
// Create browsable HTML index for a directory
//----------------------------------------------------------------------------------
void MakeHtmlOutput(Dir_t * Dir)
{
    VarList Images;
    VarList Directories;

    int IsSavedDir = 0;
    int IsRoot = 0;
            
    if (strstr(Dir->HtmlPath, "saved") != NULL) IsSavedDir = 1;
    
    Images = Dir->Images;
    Directories = Dir->Dirs;    

    // Header of file.
    printf("<html>\n<title>%s</title>\n",Dir->HtmlPath);
    printf("<head><meta charset=\"utf-8\"/>\n");
    printf("<link rel=\"stylesheet\" type=\"text/css\" href=\"styledir.css\">\n");

    // Find first image to determine aspect ratio for CSS
    int ThumbnailHeight = 100;
    for (int a=0;a<Images.NumEntries;a++){
        char * Name = Images.Entries[a].Name;
        int e = strlen(Name);
        if (e < 5 || memcmp(Name+e-4,".jpg",4)) continue; // Not an image.
        char HtmlPath[500];
        sprintf(HtmlPath, "pix/%s/%s", Dir->HtmlPath, Images.Entries[a].Name);
        float AspectRatio = ReadExifHeader(HtmlPath, NULL, NULL);
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
        "  div.ag { float:left; border-left: 1px solid black; margin-bottom:10px; min-width:130px;}\n"
        "  div.pix { float:left; width:320px; height:%dpx;}\n", ThumbnailHeight+45);
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

    printf("<p>\n");
        
    if (IsRoot){
        // Show host name and free space for root directory.
        FILE * fp = fopen("/proc/sys/kernel/hostname","r");
        char HostName[32];
        memset(HostName, 0, 32);
        fread(HostName, 1, 30, fp);
        fclose(fp);
        
        struct statvfs sv;
        statvfs("pix/", &sv);
        printf("%s: %5.1f gig ", HostName, (double)sv.f_bavail * sv.f_bsize / 1000000000);
        printf("(%4.1f%%) free<br>\n", (double)(sv.f_bavail*100.0/sv.f_blocks));
    }
    
    int SubdirImages = 0;
    if (Directories.NumEntries){
        if (!IsSavedDir){
            printf("<b>20%.2s/%.2s/%.2s</b><br>",Dir->HtmlPath,Dir->HtmlPath+2,Dir->HtmlPath+4);
        }
        PrintNavLinks(Dir, IsRoot);
        SubdirImages = ShowHourlyDirs(Dir->HtmlPath, IsRoot, Directories);
    }else{
        PrintNavLinks(Dir, IsRoot);
    }
    puts("<br>");

    if (Images.NumEntries){
        ShowThumbnailList(Dir->HtmlPath, IsSavedDir, Images);
    }

    printf("<p>\n");    
    PrintNavLinks(Dir, IsRoot);

    // Add javascript for hover-over preview when showing a whole day's worth of images
    if (SubdirImages){
        
        printf("<br><small id='prevn'></small><br>\n"
           "<a id='prevh' href=""><img id='preview' src='' width=0 height=0></a>\n");

    // Javascript
        printf("<script>\n" // Script to resize the image to the right aspect ratio
           "function sizeit(){\n"
           "  var h = 350\n"
           "  var w = h/el.naturalHeight*el.naturalWidth;\n"
           "  if (w > 1024){ w=850;h=w/el.naturalWidth*el.naturalHeight;}\n"
           "  el.width=w;el.height=h;\n"
           "}\n");
           
        printf("function mmo(str){\n"
           "   str='%s/'+str\n",Dir->HtmlPath);
        printf("   el = document.getElementById('preview')\n"
           "   el.src = 'pix/'+str\n"
           "   el.onload = sizeit\n"
           "   var eh = document.getElementById('prevh')\n"
           "   var ls = str.lastIndexOf('/')\n"
           "   eh.href = 'view.cgi?'+str.substring(0,ls)+'/#'+str.substring(ls+1)\n"
           "   var en = document.getElementById('prevn')\n"
           "   en.innerHTML = str + ' &nbsp; &nbsp; '"
           " + ' &nbsp;'+str.substring(15, 17)+':'+str.substring(17,19);"
           "}\n</script>\n");
    }
}
