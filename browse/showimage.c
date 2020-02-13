//----------------------------------------------------------------------------------
// HTML based image browser to use with imgcomp output.
//
// HTML output for browsing one image at a time.  This for the non-javascript centric
// version.  showpic.c largely replaces this, but the non-jabascript centric version
// is still of use, like for showing exif camera settings.
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
    int IsSavedDir = 0;
    char * HtmlDir;
    float AspectRatio = 1;
    VarList Images;
    Images = dir->Images;
    HtmlDir = dir->HtmlPath;
        
    if (strstr(HtmlDir, "saved/") != NULL) IsSavedDir = 1;
    

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

    {
        char PathToFile[300];
        sprintf(PathToFile, "%s/%s", HtmlDir, ImageName);
        AspectRatio = ReadExifHeader(PathToFile, NULL, NULL);
    }

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
        printf("<a href=\"tb.cgi?%s/%s$2\">[Adj]</a>\n",HtmlDir,ImageName);
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
        printf("<a href=\"view.cgi?%s%s\">%s]</a>\n", HtmlDir, IndexInto, HtmlDir+a+1);
        printf("<a href=\"view.cgi?%s/#%.4s\">[JS]</a>\n", HtmlDir, ImageName+7);

        if (!IsSavedDir){
            char SavedDir[20];
            struct stat sb;
            printf("<a id=\"save\" href=\"#\" onclick=\"GetSaveUrl('view.cgi?~%s/%s\')\">[Save]</a>\n",HtmlDir,ImageName);
            sprintf(SavedDir, "pix/saved/%.4s",HtmlDir);
            if (stat(SavedDir, &sb) == 0 && S_ISDIR(sb.st_mode)){
                printf("<a href=\"view.cgi?%s\">[View saved]</a>\n",SavedDir+4);
            }
        }
    }
}