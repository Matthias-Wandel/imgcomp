//--------------------------------------------------------------------------
// Module to make exif.c from jhead project work.
// Matthias Wandel 2015
//
// Imgcomp is licensed under GPL v2 (see README.txt)
//--------------------------------------------------------------------------
#include "jhead.h"

// Storage for simplified info extracted from file.
ImageInfo_t ImageInfo;

int ShowTags = 0;
int SupressNonFatalErrors = 0;

//--------------------------------------------------------------------------
// Report non fatal errors.  Now that microsoft.net modifies exif headers,
// there's corrupted ones, and there could be more in the future.
//--------------------------------------------------------------------------
void ErrNonfatal(const char * msg, int a1, int a2)
{
    if (SupressNonFatalErrors) return;

    fprintf(stderr,"\nNonfatal Error : ");
    fprintf(stderr, msg, a1, a2);
    fprintf(stderr, "\n");
} 


//--------------------------------------------------------------------------
// Parse the marker stream until SOS or EOI is seen;
//--------------------------------------------------------------------------
static int FindExifInFile (FILE * infile)
{
    int a;
    a = fgetc(infile);

    if (a != 0xff || fgetc(infile) != M_SOI){
        return FALSE;
    }

    for(;;){
        int itemlen;
        int marker = 0;
        int prev;
        int ll,lh, got;
        uchar * Data;

        for (a=0;;a++){
            prev = marker;
            marker = fgetc(infile);
            if (marker != 0xff && prev == 0xff) break;
            if (marker == EOF){
                fprintf(stderr, "Unexpected end of file");
                return 0;
            }
        }

        if (a > 10){
            fprintf(stderr, "Extraneous %d padding bytes before section %02X",a-1,marker);
            return 0;
        }

        if (marker != M_EXIF){
            //printf("Image did not start with exif section\n");
            return 0;
        }
  
        // Read the length of the section.
        lh = fgetc(infile);
        ll = fgetc(infile);
        if (lh == EOF || ll == EOF){
            fprintf(stderr, "Unexpected end of file");
            return 0;
        }

        itemlen = (lh << 8) | ll;

        if (itemlen < 2){
            fprintf(stderr, "invalid marker");
            return 0;
        }

        Data = (uchar *)malloc(itemlen);
        if (Data == NULL){
            fprintf(stderr, "Could not allocate memory");
            return 0;
        }

        // Store first two pre-read bytes.
        Data[0] = (uchar)lh;
        Data[1] = (uchar)ll;

        got = fread(Data+2, 1, itemlen-2, infile); // Read the whole section.
        if (got != itemlen-2){
            fprintf(stderr, "Premature end of file?");
            return 0;
        }

        if (memcmp(Data+2, "Exif", 4) == 0){
            process_EXIF(Data, itemlen);
            free(Data);
            return 1;
        }
    }
    return 0;
}

//--------------------------------------------------------------------------
// Parse the marker stream until SOS or EOI is seen;
//--------------------------------------------------------------------------
int ReadExifPart(FILE * infile)
{
    int a;
    a = FindExifInFile(infile);
    rewind(infile); // go back to start for libexif to read the image.
    return a;
}



























    