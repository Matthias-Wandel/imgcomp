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
    int IsKeepDir = 0;
    char * HtmlDir;
    float AspectRatio = 1;
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

    if (Images.NumEntries){
        char PathToFile[300];
        sprintf(PathToFile, "%s/%s", HtmlDir, Images.Entries[0].Name);
        AspectRatio = ReadExifHeader(PathToFile);
    }else{
        AspectRatio = 1.0;
    }


    printf("<div style=\"width:950px;\">");
    // Scale it to a resolution that works well on iPad.
    ShowWidth = 950;
    if (ShowWidth/AspectRatio > 535) ShowWidth = (int)535 * AspectRatio;
    ShowHeight = (int)(ShowWidth/AspectRatio+0.5);

    printf("<center>");
    printf("<img id='view' width=%d height=%d",ShowWidth, ShowHeight);
    printf(" src=\"pix/%s/%s\" usemap=\"#prevnext\"></a><br>\n\n",HtmlDir,ImageName);

    // Make left and right parts of the image clickable as previous and next.
    if (Images.Entries){
        printf("<map name=\"prevnext\">\n");
        printf("  <area shape=\"rect\" coords= \"0,0,%d,%d\" ",ShowWidth/4, ShowHeight);
        printf("onmousedown=\"PicMouseDown(-1)\" onmouseup=\"PicMouseUp()\">\n");
        printf("  <area shape=\"rect\" coords=\"%d,0,%d,%d\" ",(ShowWidth*3)/4, ShowWidth, ShowHeight);
        printf("onmousedown=\"PicMouseDown(1)\" onmouseup=\"PicMouseUp()\">\n");       
        printf("</map>\n");
    }
    
    printf("<span id='links'>links go here</span>\n");

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
        printf("<a href=\"#\" onclick=\"ShowBig()\">[Big]</a>\n",HtmlDir,ImageName);
        printf("<a href=\"#\" onclick=\"ShowAdj()\">[Adj]</a>\n",HtmlDir,ImageName);
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
            printf("<a id=\"save\" href=\"#\" onclick=\"DoSavePic()\">[Save]</a>\n",HtmlDir,ImageName);
            sprintf(KeepDir, "pix/keep/%.4s",HtmlDir);
            if (stat(KeepDir, &sb) == 0 && S_ISDIR(sb.st_mode)){
                printf("<a href=\"view.cgi?%s\">[View saved]</a>\n",KeepDir+4);
            }
        }
    }
   
    printf("\n<script type=\"text/javascript\">\n");
    printf("piclist = [");
    for (a=0;a<(int)Images.NumEntries;a++){
        if (a){
            putchar(',');
            if (a % 5 == 0) putchar('\n');
        }

        char * Name = Images.Entries[a].Name+7;
        printf("\"%.9s\"",Name);
    }
    printf("];\n");
    printf("pixpath = \"pix/\"\n");
    printf("subdir = \"%s/\"\n",dir);
    printf("prefix = \"%.7s\"\n",Images.Entries[0].Name);
    printf("</script>\n");
    printf("<script type=\"text/javascript\" src=\"showpic.js\"></script>\n");

}