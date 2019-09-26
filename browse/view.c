//----------------------------------------------------------------------------------
// Main module for web image browser for parsing images produced
// by my imgcomp program on raspberery pi.
//----------------------------------------------------------------------------------
#include <stdio.h>
#include <errno.h>
#include <memory.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include "view.h"
#include "../src/jhead.h"
#ifdef _WIN32
    #include <io.h>
    #include <direct.h>
#else
    #include <sys/stat.h>
    #include <unistd.h>
#endif

static char * FileExtensions[] = {"jpg","jpeg","txt","html",NULL};
char * ImageExtensions[] = {"jpg","jpeg",NULL};
float AspectRatio = 4.0/3;

//----------------------------------------------------------------------------------
// Process one directory.  Returns pointer to summary.
//----------------------------------------------------------------------------------
Dir_t * CollectDir(char * HtmlPath)
{
    Dir_t * Dir;
    VarList * Subdirs;
    VarList * Images;
	char DirName[300];

    Dir = (Dir_t *) malloc(sizeof(Dir_t));
    memset(Dir, 0, sizeof(Dir_t));

    if (Dir == NULL){
        printf("out of memory\n");
        // Very bad.  Don't attempt to fix it.
        exit(-1);
    }

    Subdirs = &Dir->Dirs;
    Images = &Dir->Images;

    if (strlen(HtmlPath) > 250){
        printf("Path string is too long\n");
        exit(0);
    }
	strcpy(Dir->HtmlPath, HtmlPath);

	sprintf(DirName, "pix/%s",HtmlPath);
	CollectDirectory(DirName, Images, Subdirs, FileExtensions);


	// Look for previous and next.
	if (strlen(HtmlPath) > 2){
		char ThisDir[100];
		VarList Siblings;
		unsigned a;
		int LastSlash = 0;
		Siblings.NumEntries = Siblings.NumAllocated = 0;
		Siblings.Entries = NULL;

		for (a=0;HtmlPath[a] && a < 99;a++){
			if (HtmlPath[a-1] == '/' && HtmlPath[a]) LastSlash = a;
		}
		memcpy(Dir->Parent, HtmlPath, LastSlash);
		Dir->Parent[LastSlash-1] = '\0';
		strcpy(ThisDir, HtmlPath+LastSlash);
		a = strlen(ThisDir);
		if (ThisDir[a-1] == '/') ThisDir[a-1] = '\0';

		sprintf(DirName, "pix/%s",Dir->Parent);

		CollectDirectory(DirName, NULL, &Siblings, NULL);

		for (a=0;a<Siblings.NumEntries;a++){
			char * slash;
			//printf("%s<br>\n",Siblings.Entries[a].Name);
			slash = "";
			if (Dir->Parent[0]) slash = "/";
			if (strcmp(Siblings.Entries[a].Name, ThisDir) == 0){
				if (a > 0){
					snprintf(Dir->Previous, 200, "%.90s%.1s%.90s", Dir->Parent, slash, Siblings.Entries[a-1].Name);
				}
				if (a < Siblings.NumEntries-1){
					snprintf(Dir->Next, 200, "%.90s%.1s%.90s", Dir->Parent, slash, Siblings.Entries[a+1].Name);
				}
			}
		}

	}
    return Dir;
}

//----------------------------------------------------------------------------------
// Read exif header of an image to get aspect ratio and other info.
//----------------------------------------------------------------------------------
static void ReadExifHeader(char * ImagePath)
{
    char FileName[300];
    FILE * file;
    sprintf(FileName, "pix/%s",ImagePath);
    file = fopen(FileName, "rb");
    if(file) {
        ReadExifPart(file);
    }

    if (ImageInfo.Width > 10 && ImageInfo.Height > 10){
        AspectRatio = (float)ImageInfo.Width / ImageInfo.Height;
    }
}


//----------------------------------------------------------------------------------
// Collect info for making jpeg view.
//----------------------------------------------------------------------------------
void DoJpegView(char * ImagePath)
{
    char HtmlDir[300];
    char HtmlFile[300];
    int a;
    int lastslash = 0;
    VarList Images;

    ReadExifHeader(ImagePath);

    memset(&Images, 0, sizeof(Images));

    for (a=0;ImagePath[a];a++){
        if (ImagePath[a] == '/') lastslash = a;
    }
    sprintf(HtmlDir, "pix/%s",ImagePath);
    HtmlDir[lastslash+1+4] = '\0';

    strcpy(HtmlFile, ImagePath+lastslash+1);

    //printf("html dir: %s<br>\nFile:%s<br><hr>\n",HtmlDir+4, HtmlFile);

    CollectDirectory(HtmlDir, &Images, NULL, ImageExtensions);

    MakeImageHtmlOutput(HtmlFile, HtmlDir+4, Images);
    free(Images.Entries);


    printf("<pre>");
    // Delete uninteresting fields of exif info to not show them.
    ImageInfo.FlashUsed = -1;
    ImageInfo.MeteringMode = 0;
    ImageInfo.ExposureProgram = 0;
    ImageInfo.CameraMake[0] = 0;
    ImageInfo.FocalLength = 0;
    ImageInfo.ApertureFNumber = 0;
    ImageInfo.Whitebalance = -1;
    ShowImageInfo(0);
}

//----------------------------------------------------------------------------------
// If no location specified, redirect to today's directory, if it exists.
//----------------------------------------------------------------------------------
void RedirectToToday()
{
    char RedirectDir[20];
    time_t now;
    struct stat ignore;

    //printf("Content-Type: text/html\n\n<html>\n"); // html header
    now = time(NULL); // Log names are based on this time, need before images.
    strftime(RedirectDir, 20, "pix/%m%d", localtime(&now));

    RedirectDir[7] += 1;
    // Check if directory exists.
    if (stat(RedirectDir, &ignore)){
         // Doesn't exist.  Root instead.
         strcpy(RedirectDir, "pix/");
    }

    printf ("Location:  view.cgi?%s/\n\n",RedirectDir+4);
}

//----------------------------------------------------------------------------------
// Save image feature.
//----------------------------------------------------------------------------------
void DoSaveImage(char * QueryString, char * HtmlPath)
{
    int a;
    int wi;
    char NewName[100];
    char FromName[100];

    strcpy(NewName, "pix/keep");
    for (a=0,wi=8;HtmlPath[a];a++){
        if (HtmlPath[a] == '/') wi = 8;
        if (HtmlPath[a] == ' ' || HtmlPath[a] == '.' || HtmlPath[a] == '\0'){
            NewName[wi++] = '\0';
            break;
        }else{
            NewName[wi++] = HtmlPath[a];
        }
    }
    strcat(NewName, ".jpg");
    sprintf(FromName, "../%s",HtmlPath+1);

    //printf("New name: %s<br>\n",NewName);
    //printf("From name: %s<p>\n",FromName);

    if (symlink(FromName, NewName)){
        printf("Symlink failed<p>\n");
    }else{
        printf("Saved image<p>\n");
    }

    printf ("<p><a href=\"view.cgi?/keep/\">View saved</a>\n");

    printf ("<p><a href=\"view.cgi?%s\">Back</a><br>\n",QueryString+1);

}

//----------------------------------------------------------------------------------
// Main
//----------------------------------------------------------------------------------
int main(int argc, char ** argv)
{
    int a;
    char * QueryString;
    char HtmlPath[300];
    int lp;
    QueryString = getenv("QUERY_STRING");

    if (!QueryString || QueryString[0] == '\0'){
        RedirectToToday();
        return 0;
    }

    printf("Content-Type: text/html\n\n<html>\n"); // html header

    HtmlPath [0] = '\0';
    if (QueryString){
        int d;

        // Unescape for "%20"
        for (a=0,d=0;;a++){
            if (QueryString[a] == '%' && QueryString[a+1] == '2' && QueryString[a+2] == '0'){
                HtmlPath[d++] = ' ';
                a+= 2;
            }else{
                HtmlPath[d++] = QueryString[a];
            }
            if (QueryString[a] == 0) break;
        }
    }

    if (QueryString[0] == '~'){
        DoSaveImage(QueryString, HtmlPath);
        return 0;
    }
    
    //printf("QUERY_STRING=%s<br>\n",QueryString);
    //printf("HTML path = %s\n",HtmlPath);

    lp = 0;
    for (a=0;HtmlPath[a];a++){
        if (HtmlPath[a] == '.') lp = a;
    }
    if (lp && (a-lp == 4 || a-lp == 5) && HtmlPath[lp+1] == 'j' && HtmlPath[a-1] == 'g'){
        // Path ends with .jpg
        DoJpegView(HtmlPath);

    }else{
        // Doesn't end with .jpg.  Its a directory.
        int l;
        Dir_t * Col;
        l = strlen(HtmlPath);
        if (l && HtmlPath[l-1] != '/'){
            HtmlPath[l] = '/';
            HtmlPath[l+1] = '\0';
        }

        Col = CollectDir(HtmlPath);

        // Find first image and read exif header of it to get aspect ratio.
        for (a=0;a<Col->Images.NumEntries;a++){
            int l = strlen(Col->Images.Entries[a].Name);
            if (Col->Images.Entries[a].Name[l-1] == 'g'){
                sprintf(HtmlPath, "%s%s", Col->HtmlPath, Col->Images.Entries[0].Name);
                ReadExifHeader(HtmlPath);
                break;
            }
        }

        MakeHtmlOutput(Col);

        free(Col->Dirs.Entries);
        free(Col->Images.Entries);
        free(Col);
    }
    return 0;
}

