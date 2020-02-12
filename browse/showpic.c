//----------------------------------------------------------------------------------
// HTML output for browsing one image at a time
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
    int PicWidth = 0;
    int PicHeight = 0;

    if (strstr(HtmlDir, "saved/") != NULL) IsSavedDir = 1;

    printf("<title>%s</title>\n",ImageName);
    printf("<head><meta charset=\"utf-8\"/>\n");

    printf("<style type=text/css>\n"
           "  body { font-family: sans-serif; font-size: 22;}\n"
           "  img { vertical-align: middle; margin-bottom: 5px; }\n"
           "  p {margin-bottom: 0px}\n"
           "  a {text-decoration: none;}\n"
           "  button {font-size: 20px;}\n"
           "</style></head>\n\n");

    if (Images.NumEntries){
        char PathToFile[300];
        sprintf(PathToFile, "%s/%s", HtmlDir, Images.Entries[0].Name);
        AspectRatio = ReadExifHeader(PathToFile, &PicWidth, &PicHeight);
    }else{
        AspectRatio = 1.0;
    }

    printf("<div style=\"width:950px;\">");
    // Scale it to a resolution that works well on iPad.
    ShowWidth = 950;
    if (ShowWidth/AspectRatio > 535) ShowWidth = (int)535 * AspectRatio;
    ShowHeight = (int)(ShowWidth/AspectRatio+0.5);

    printf("<center>\n");
    printf("<span id='image'>image goes here</span>\n");
    printf("<br>\n<span id='links'>links goes here</span>\n");

    printf("</center></div>");

    printf("<p>\n");
    {
        char IndexInto[8];
        IndexInto[0] = 0;
        if (strlen(ImageName) >= 10){
            IndexInto[0] = '#';
            IndexInto[1] = ImageName[7];
            IndexInto[2] = ImageName[8];
            IndexInto[3] = '\0';
        }

        printf("<button onclick=\"ShowBig()\">Big</button>\n");
        printf("<button onclick=\"ShowAdj()\">Adj</button>\n");
        printf("<button onclick=\"ShowLog()\">Log</button>\n");
        printf("<button onclick=\"ShowOld()\">Detail</button>\n");

        for (a=0;;a++){
            if (HtmlDir[a] == '/') break;
            if (HtmlDir[a] == '\0'){
                a = 0; break;
            }
        }

        if (!IsSavedDir){
            char SavedDir[20];
            struct stat sb;
            printf("<button id='save' onclick=\"DoSavePic()\">Save</button>\n");
            sprintf(SavedDir, "pix/saved/%.4s",HtmlDir);
            if (stat(SavedDir, &sb) == 0 && S_ISDIR(sb.st_mode)){
                printf("<a href=\"view.cgi?%s\">[View saved]</a>\n",SavedDir+4);
            }
        }
        printf("<a href=\"view.cgi?/\">[Dir</a>:");
        printf("<a href=\"view.cgi?%.*s\">", a, HtmlDir);
        printf("%.*s</a>/", a, HtmlDir);
        printf("<a href=\"view.cgi?%s/%s\">%s]</a>\n", HtmlDir, IndexInto, HtmlDir+a+1);

    }

    int npic = 0;
    char * Prefix = NULL;
    int prefixlen = 0;
    for (a=0;a<(int)Images.NumEntries;a++){
        char * Name = Images.Entries[a].Name;
        int e = strlen(Name);
        if (e < 5 || memcmp(Name+e-4,".jpg",4)) continue;

        if (Prefix == NULL){
            Prefix = Name;
            prefixlen = strlen(Name)-4;
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

    printf("\n<script type=\"text/javascript\">\n");
    printf("pixpath=\"pix/\"\n");
    printf("subdir=\"%s/\"\n",dir->HtmlPath);
    printf("prefix=\"%.*s\"\n",prefixlen, Prefix);
    printf("piclist = [");

    for (a=0;a<(int)Images.NumEntries;a++){
        char * Name = Images.Entries[a].Name;
        int e = strlen(Name);
        if (e < 5 || memcmp(Name+e-4,".jpg",4)) continue;
        e -= 4;

        if (npic){
            putchar(',');
            if (npic % 5 == 0) putchar('\n');
        }
        printf("\"%.*s\"",e-prefixlen,Name+prefixlen);
        npic++;
    }
    if (npic == 0) printf("'----'");
    printf("];\n\n");
    printf("isSavedDir=%d\n",IsSavedDir);
    printf("PrevDir=\"%s\";NextDir=\"%s\"\n",dir->Previous, dir->Next);
    printf("PicWidth=%d;PicHeight=%d\n",PicWidth, PicHeight);
        printf("</script>\n");
    printf("<script type=\"text/javascript\" src=\"showpic.js\"></script>\n");

}