//----------------------------------------------------------------------------------
// HTML based image browser to use with imgcomp output.
//
// Makes HTML output for flipping through an hour's worth of images, javascript based version.
// This code builds a HTML page, which contains a basic layout and, a list of images
// for javascript and some variables.
// The page includes showpic.js to do most of the work.  Flips thru images with
// only reloading the appropriate jpeg image, no flicker.
//
// Imgcomp and html browsing tool is licensed under GPL v2 (see README.txt)
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
// Create a HTML to flip thru a directory of images
//----------------------------------------------------------------------------------
void MakeViewPage(char * ImageName, Dir_t * dir)
{
    int IsSavedDir = 0;
    char * HtmlDir;
    VarList Images;
    Images = dir->Images;
    HtmlDir = dir->HtmlPath;

    if (strstr(HtmlDir, "saved/") != NULL) IsSavedDir = 1;

    printf("<title>%s</title>\n",ImageName);
    printf("<head><meta charset=\"utf-8\"/>\n");

    printf("<style type=text/css>\n"
           "  body { font-family: sans-serif; font-size: 20; -webkit-user-select: none; -webkit-touch-callout: none;}\n"
           "  img { vertical-align: middle; margin-bottom: 5px; }\n"
           "  p {margin-bottom: 0px}\n"
           "  a {text-decoration: none;}\n"
           "  button {-webkit-appearance: none; font-size: 20px; padding-left:9px; padding-right:8px; padding-bottom:3px;background-color:#E8E8E8;border-radius:8px;}\n"
           "  img {-webkit-user-select: none; -webkit-touch-callout: none;}\n"
           "</style></head>\n\n");

    printf("<center>\n");
    
    printf("<img id='view' width=640 height=480 src=''>\n");
    
    printf("<br>");

    // Output HTML code for the buttons below the row of navigation links
    {
        printf("<span id='this'>this</span>\n");
        printf("<button id='big' onclick=\"ShowBigClick()\">Enlarge</button>\n");
        printf("<button id='bright' onclick=\"ShowBrightClick()\">Brighten</button>\n");
        printf("<button onclick=\"ShowDetailsClick()\">Details</button>\n");
        printf("<button id='play' onclick=\"PlayButtonClick()\">Play</button>\n");

        if (!IsSavedDir){
            char SavedDir[20];
            struct stat sb;
            printf("<button id='save' onclick=\"SavePicClick()\">Save</button>\n");
            sprintf(SavedDir, "pix/saved/%.4s",HtmlDir);
            if (stat(SavedDir, &sb) == 0 && S_ISDIR(sb.st_mode)){
                printf("&nbsp;<a href=\"view.cgi?%s\">Saved</a>&nbsp;\n",SavedDir+4);
            }
        }

        int l;
        if ((l = strlen(HtmlDir)) && HtmlDir[l-1] == '/'){
            printf("<a href=\"view.cgi?%.*s\">Thumbnails</a>&nbsp;\n",l-1,HtmlDir);
        }

        // Link to each level of subdirectory.
        printf("<a href=\"view.cgi?/\">Dir</a>:");
        int pa = 0;
        for (int a=0;;a++){
            if (HtmlDir[a] == '/' || HtmlDir[a] == '\0' || HtmlDir[a] == '#'){
                printf("<a href=\"view.cgi?%.*s\">", a, HtmlDir);
                printf("%.*s</a>",a-pa, HtmlDir+pa);
                if (HtmlDir[a] != '/' || HtmlDir[a+1] == '\0' || HtmlDir[a+1] == '#') break;
                putchar('/');
                pa = a+1;
            }
        }
        
        printf("&nbsp;");
        if (dir->Previous[0]) printf("<a href='#' id='prevdir'>&lt;&lt;</a>");
        if (dir->Next[0]){
            printf("&nbsp;<a href='#' id='nextdir'>>></a>");
        }else{
            // Maybe only show link to realtime.html if looking at the current day?
            printf("&nbsp;<a href='realtime.html'>Rt</a>");
        }
        printf("\n");
    }
    printf("<br>\n<canvas id='hist' width='960' height='35' "
           "style=\"margin-top: 5px; border-width: 1px; border:1px solid #000000;\"></canvas>\n");

    //printf("</center>dbg:[<span id='dbg'>xxx</span>]\n");
    // check how many characters all the filenames have in common (typically 7)
    int npic = 0;
    char * Prefix = NULL;
    int prefixlen = 0;

    printf("\n<script type=\"text/javascript\">\n");
    printf("pixpath=\"pix/\"\n");
    printf("subdir=\"%s\"\n",dir->HtmlPath);
    printf("prefix=\"%.*s\"\n",prefixlen, Prefix);
    printf("piclist = [");

    // Output list of images in the hour that we can flip through.
    int HasLog = 0;

    for (int a=0;a<(int)Images.NumEntries;a++){
        char * Name = Images.Entries[a].Name;
        if (strcmp(Name, "Log.html") == 0) HasLog = 1;
        if (!NameIsImage(Name)) continue;

        int e = strlen(Name);
        e -= 4;

        if (npic){
            putchar(',');
            if (npic % 5 == 0) putchar('\n');
        }
        printf("\"%s\"",Name+prefixlen);
        npic++;
    }
    
    printf("];\n\n");
    
    // Dump some variables for the javascript code.
    printf("hasLog=%d\n",HasLog);
    printf("isSavedDir=%d\n",IsSavedDir);
    printf("PrevDir=\"%s\";NextDir=\"%s\"\n",dir->Previous, dir->Next);
    printf("</script>\n");
    
    // Include the showpic.js javascript file.
    printf("<script type=\"text/javascript\" src=\"showpic.js\"></script>\n");
}
