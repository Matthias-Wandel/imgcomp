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
    int DirIndex;
    int FoundMatch;
    int From,To;
    int IsSavedDir = 0;
    char * HtmlDir;
    VarList Images;
    Images = dir->Images;
    HtmlDir = dir->HtmlPath;
    int PicWidth = 0;
    int PicHeight = 0;

    if (strstr(HtmlDir, "saved/") != NULL) IsSavedDir = 1;

    printf("<title>%s</title>\n",ImageName);
    printf("<head><meta charset=\"utf-8\"/>\n");

    printf("<style type=text/css>\n"
           "  body { font-family: sans-serif; font-size: 22; -webkit-user-select: none; -webkit-touch-callout: none;}\n"
           "  img { vertical-align: middle; margin-bottom: 5px; }\n"
           "  p {margin-bottom: 0px}\n"
           "  a {text-decoration: none;}\n"
           "  button {font-size: 18px;}\n"
           "  img {-webkit-user-select: none; -webkit-touch-callout: none;}\n"
                    
           "</style></head>\n\n");

    printf("<center>\n");
    
    printf("<img id='view' width=640 height=480 src=''>\n");
   
    
    printf("<br>\n<span id='links'>links goes here</span>\n");
    printf("<br>");

    // Output HTML code for the buttons below the row of navigation links
    {
        char IndexInto[8];
        IndexInto[0] = 0;
        if (strlen(ImageName) >= 10){
            IndexInto[0] = '#';
            IndexInto[1] = ImageName[7];
            IndexInto[2] = ImageName[8];
            IndexInto[3] = '\0';
        }

        printf("<button id='big' onclick=\"ShowBig()\">Enlarge</button>\n");
        printf("<button id='bright' onclick=\"ShowBright()\">Brighten</button>\n");
        printf("<button onclick=\"ShowDetails()\">Details</button>\n");
        printf("<button id='play' onclick=\"PlayButton()\">Play</button>\n");

        if (!IsSavedDir){
            char SavedDir[20];
            struct stat sb;
            printf("<button id='save' onclick=\"DoSavePic()\">Save</button>\n");
            sprintf(SavedDir, "pix/saved/%.4s",HtmlDir);
            if (stat(SavedDir, &sb) == 0 && S_ISDIR(sb.st_mode)){
                printf("&nbsp;<a href=\"view.cgi?%s\">View saved</a>&nbsp;\n",SavedDir+4);
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
        if (dir->Previous[0]) printf("<a href='view.cgi?%s/'>&lt;&lt;</a>",dir->Previous);
        if (dir->Next[0]) printf("&nbsp;<a href='view.cgi?%s/'>>></a>",dir->Next);
        printf("\n");
    }
    printf("<br>Actagram:\n<b><span id='actagram' style=\"font-family: courier, \'courier new\', monospace;\">Actagram here</span></b>\n");

    // check how many characters all the filenames have in common (typically 7)
    int npic = 0;
    char * Prefix = NULL;
    int prefixlen = 0;
    for (int a=0;a<(int)Images.NumEntries;a++){
        char * Name = Images.Entries[a].Name;
        int e = strlen(Name);
        if (e < 5 || memcmp(Name+e-4,".jpg",4)) continue;

        if (PicWidth == 0){
            // Need to get image size (assume all the same size)
            char PathToFile[300];
            sprintf(PathToFile, "%s/%s", HtmlDir, Images.Entries[a].Name);
            ReadExifHeader(PathToFile, &PicWidth, &PicHeight);
        }

        if (Prefix == NULL){
            Prefix = Name;
            prefixlen = strlen(Name)-4;
            if (prefixlen > 7) prefixlen = 7; // prefix length must not be more than 7!
            continue;
        }

        if (memcmp(Prefix, Name, prefixlen)){
            for (int b=0;b<prefixlen;b++){
                if (Name[b] != Prefix[b]){
                    prefixlen = b;
                    break;
                }
            }
        }
        if (prefixlen == 0) break;
    }
    prefixlen = prefixlen >= 7 ? 7 : 0;

    printf("\n<script type=\"text/javascript\">\n");
    printf("pixpath=\"pix/\"\n");
    printf("subdir=\"%s\"\n",dir->HtmlPath);
    printf("prefix=\"%.*s\"\n",prefixlen, Prefix);
    printf("piclist = [");

    // Output list of images in the hour that we can flip through.
    int HasLog = 0;
    for (int a=0;a<(int)Images.NumEntries;a++){
        char * Name = Images.Entries[a].Name;
        int e = strlen(Name);
        if (e < 5 || memcmp(Name+e-4,".jpg",4)){
            if (strcmp(Name, "Log.html") == 0) HasLog = 1;
            continue;
        }
        e -= 4;

        if (npic){
            putchar(',');
            if (npic % 5 == 0) putchar('\n');
        }
        printf("\"%.*s\"",e-prefixlen,Name+prefixlen);
        npic++;
    }
    
    printf("];\n\n");
    
    // Dump some variables for the javascript code.
    printf("hasLog=%d\n",HasLog);
    printf("isSavedDir=%d\n",IsSavedDir);
    printf("PrevDir=\"%s\";NextDir=\"%s\"\n",dir->Previous, dir->Next);
    printf("PicWidth=%d;PicHeight=%d\n",PicWidth, PicHeight);
    printf("</script>\n");
    
    // Include the showpic.js javascript file.
    printf("<script type=\"text/javascript\" src=\"showpic.js\"></script>\n");
}
