//----------------------------------------------------------------------------------
// HTML based image browser to use with imgcomp output.
//
// Main module for web image browser for parsing images produced
// by my imgcomp program on raspberery pi.
// Parses parameters, collects info, then depending on what is requested calls
// appropirate module to generate the HTML.
//
// Imgcomp and html browsing tool is licensed under GPL v2 (see README.txt)
//----------------------------------------------------------------------------------
#include <stdio.h>
#include <errno.h>
#include <memory.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include "view.h"
#include "../src/jhead.h"
#include <sys/stat.h>
#include <unistd.h>

static char * FileExtensions[] = {"jpg","jpeg","txt","html","mp4",NULL};
       char * ImageExtensions[] = {"jpg","jpeg",NULL};
       
//----------------------------------------------------------------------------------
// Process one directory.  Returns pointer to summary.
//----------------------------------------------------------------------------------
static Dir_t * CollectDir(char * HtmlPath, int ImagesOnly)
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
	CollectDirectory(DirName, Images, Subdirs, ImagesOnly? ImageExtensions:FileExtensions);


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
float ReadExifHeader(char * ImagePath, int * width, int * height)
{
    char FileName[320];
    FILE * file;
    sprintf(FileName, "pix/%s",ImagePath);
    if((file = fopen(FileName, "rb")) != NULL) {
        ReadExifPart(file);
    }else{
        printf("Failed to read %s<br>\n", FileName);
    }

    if (ImageInfo.Width > 10 && ImageInfo.Height > 10){
        if (width) *width = ImageInfo.Width;
        if (height) *height = ImageInfo.Height;
        return (float)ImageInfo.Width / ImageInfo.Height;
    }else{
        return 1; // Bogus but nonzero value.
    }
}


//----------------------------------------------------------------------------------
// Collect info for making jpeg view.
//----------------------------------------------------------------------------------
void DoJpegView(char * ImagePath)
{
    char HtmlFile[150];
    char HtmlPath[150];
    int a;
    int lastslash = 0;
    Dir_t * dir;

    for (a=0;ImagePath[a];a++){
        if (ImagePath[a] == '/') lastslash = a;
    }

    strncpy(HtmlFile, ImagePath+lastslash+1,149);
    HtmlFile[149] = 0;

    strncpy(HtmlPath, ImagePath,149);
    HtmlPath[149] = 0;
    HtmlPath[lastslash] = '\0';
    dir = CollectDir(HtmlPath, 1);

    ReadExifHeader(ImagePath, NULL, NULL);

    MakeImageHtmlOutput(HtmlFile, dir, (float)ImageInfo.Width / ImageInfo.Height);
        
    free(dir->Dirs.Entries);
    free(dir->Images.Entries);
    free(dir);

    printf("<p>Date, time: %s<br>\n",ImageInfo.DateTime);
    printf("Image size: %d x %d<br>\n",ImageInfo.Width, ImageInfo.Height);

    if (ImageInfo.ExposureTime || ImageInfo.ISOequivalent || ImageInfo.ApertureFNumber){
        printf("Exposure: ");
        if (ImageInfo.ExposureTime){
            if (ImageInfo.ExposureTime <= 0.5){
                printf("1/%ds\n",(int)(0.5 + 1/ImageInfo.ExposureTime));
            }else{
                printf("%1.1fs\n",ImageInfo.ExposureTime);
            }
        }
        if (ImageInfo.ApertureFNumber){
            printf("f/%3.1f\n",(double)ImageInfo.ApertureFNumber);
        }
        if (ImageInfo.ISOequivalent != 0){
            printf("ISO:%2d\n",(int)ImageInfo.ISOequivalent);
        }
        printf("<br>\n");
    }

    if (ImageInfo.FocalLength35mmEquiv){
        printf("Focal length equivalent: f(35)=%dmm",ImageInfo.FocalLength35mmEquiv);
    }
    
    
    struct stat buf;
    char FileName[320];
    sprintf(FileName, "pix/%s",ImagePath);
    stat(FileName, &buf);
    printf("File size: %d bytes<br>\nLocation: %s<br>\n",(int)buf.st_size, ImagePath);
    
    printf("\n");
    
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
    strftime(RedirectDir, 20, "pix/%y%m%d", localtime(&now));

    // Check if directory exists.
    if (stat(RedirectDir, &ignore)){
         // Doesn't exist.  Go to root directory instead.
         strcpy(RedirectDir, "    /");
    }

    printf ("Location:  view.cgi?%s\n\n",RedirectDir+4);
}

//----------------------------------------------------------------------------------
// Save image feature.
//----------------------------------------------------------------------------------
void DoSaveImage(char * QueryString, char * HtmlPath)
{
    int a, wi;
    char NewName[100];
    char FromName[100];
    char TempString[20];
    char NewDir[20];
    struct stat sb;
        
    for (a=0,wi=0;HtmlPath[a];a++){
        if (HtmlPath[a] == '/'){
            wi = 0;
        }else if (HtmlPath[a] == ' ' || HtmlPath[a] == '.' || HtmlPath[a] == '\0' || wi >= 19){
            TempString[wi++] = '\0';
            break;
        }else{
            TempString[wi++] = HtmlPath[a];
        }
    }
    
    strcpy(NewDir, "pix/saved/");
    if (stat(NewDir, &sb) != 0){
        mkdir(NewDir, 0777);
    }
    
    strncat(NewDir, HtmlPath+1, 4);
    
    if (stat(NewDir, &sb) == 0){
        if (S_ISDIR(sb.st_mode)) {
            // Its a directory.  Ok.
        }else{
            // Uh oh, it exists but is not a directory.
            printf("Fail: Can't make directory");
            return;
        }
    }else{
        // Doesn't exist.  Make directory.
        if (mkdir(NewDir, 0777)){
            printf("Fail: mkdir");
            return;
        }
    }
    
    sprintf(NewName, "%s/%s.jpg",NewDir,TempString);
    sprintf(FromName, "pix/%s",HtmlPath+1);
    //printf("New name: %s<br>\n",NewName);
    //printf("From name: %s<p>\n",FromName);

    if (link(FromName, NewName)){
        printf("Fail:%s",strerror( errno ));
    }else{
        printf("Saved");
    }
}

//----------------------------------------------------------------------------------
// Dump latest qauired image in /ramdisk.
//----------------------------------------------------------------------------------
void ShowLatestJpg(void)
{
    VarList files;

    memset(&files, 0, sizeof(files));
    CollectDirectory("/ramdisk", &files, NULL, ImageExtensions);
    
    if (files.NumEntries){
        char InName[100];
        sprintf(InName, "/ramdisk/%s",files.Entries[files.NumEntries-1].Name);
        
        printf("Content-Type: image/jpg\n\n");
        //printf("input file name = %s\n",InName);
        // Now dump it to stdout.
        {
            unsigned char Chunk[4096];
            FILE * infile = fopen(InName, "rb");
            for (;;){
                int nr = fread(Chunk, 1, 4096, infile);
                if (nr <= 0) break;
                fwrite(Chunk, 1, nr, stdout);
            }
        }
    }
    free(files.Entries);
}

//----------------------------------------------------------------------------------
// Main
//----------------------------------------------------------------------------------
int main(int argc, char ** argv)
{
    int a;
    char * QueryString;
    char HtmlPath[300];
    int lp, d;
    QueryString = getenv("QUERY_STRING");

    if (!QueryString || QueryString[0] == '\0'){
        RedirectToToday();
        return 0;
    }
    
    if (memcmp(QueryString, "now.jpg",7) == 0){
        ShowLatestJpg();
        return 0;
    }

    printf("Content-Type: text/html\n\n"); // html header

    if (memcmp(QueryString, "actagram", 8) == 0){
        int all = 0;
        int h24 = 0;
        if (strcmp(QueryString, "actagram,all") == 0) all = 1;
        if (strstr(QueryString, "24") != NULL) h24 = 1;
        ShowActagram(all,h24);
        return 0;
    }

    HtmlPath [0] = '\0';

    // Unescape for "%20"
    a = 0;
    if (QueryString[a] == '/') a++;
    for (d=0;;a++){
        if (QueryString[a] == '%' && QueryString[a+1] == '2' && QueryString[a+2] == '0'){
            HtmlPath[d++] = ' ';
            a+= 2;
        }else{
            HtmlPath[d++] = QueryString[a];
        }
        if (QueryString[a] == 0) break;
    }

    if (QueryString[0] == '~'){
        DoSaveImage(QueryString, HtmlPath);
        return 0;
    }
    
    //printf("QUERY_STRING=%s<br>\n",QueryString);
    //printf("HTML path = %s<br>\n",HtmlPath);

    lp = 0;
    for (a=0;HtmlPath[a];a++){
        if (HtmlPath[a] == '.') lp = a;
    }
    if (lp && (a-lp == 4 || a-lp == 5) && HtmlPath[lp+1] == 'j' && HtmlPath[a-1] == 'g'){
        // Path ends with .jpg
        DoJpegView(HtmlPath);

    }else{
        // Doesn't end with .jpg.  Its a directory.
        Dir_t * Col;
        int a;
        int ts = 0;
        for (a=0;;a++){
            if (HtmlPath[a] == '#' || HtmlPath[a] == '\0') break;
        }
        if (a > 1 && HtmlPath[a-1] == '/') ts = 1;

        Col = CollectDir(HtmlPath, 0);
        if (ts){
            // Trailing slash means new javascript view
            MakeViewPage(HtmlPath, Col);   
        }else{
            MakeHtmlOutput(Col);
        }

        free(Col->Dirs.Entries);
        free(Col->Images.Entries);
        free(Col);
    }
    return 0;
}

