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
// Get 16 bits motorola order (always) for jpeg header stuff.
//--------------------------------------------------------------------------
static int Get16m(const void * Short)
{
    return (((uchar *)Short)[0] << 8) | ((uchar *)Short)[1];
}

//--------------------------------------------------------------------------
// Process a SOFn marker.  This is useful for the image dimensions
//--------------------------------------------------------------------------
static void process_SOFn (const uchar * Data, int marker)
{
    int data_precision, num_components;

    data_precision = Data[2];
    ImageInfo.Height = Get16m(Data+3);
    ImageInfo.Width = Get16m(Data+5);
    num_components = Data[7];

    if (num_components == 3){
        //ImageInfo.IsColor = 1;
    }else{
        //ImageInfo.IsColor = 0;
    }

    //ImageInfo.Process = marker;

    if (ShowTags){
        printf("JPEG image is %uw * %uh, %d color components, %d bits per sample\n",
                   ImageInfo.Width, ImageInfo.Height, num_components, data_precision);
    }
}



//--------------------------------------------------------------------------
// Parse the marker stream until SOS or EOI is seen;
//--------------------------------------------------------------------------
static int FindExifInFile (FILE * infile)
{
    int a;
    int have_exif = 0, have_sof= 0;
    int SectionsRead = 0;
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
        
        SectionsRead += 1;
        
        switch(marker){        
            case M_EXIF:
                if (memcmp(Data+2, "Exif", 4) == 0){
                    process_EXIF(Data, itemlen);
                    have_exif = 1;
                }
                break;
            case M_SOF0: 
            case M_SOF1: 
            case M_SOF2: 
            case M_SOF3: 
            case M_SOF5: 
            case M_SOF6: 
            case M_SOF7: 
            case M_SOF9: 
            case M_SOF10:
            case M_SOF11:
            case M_SOF13:
            case M_SOF14:
            case M_SOF15:
                process_SOFn(Data, marker);
                have_sof = 1;
                break;
                
            case M_SOS:
                SectionsRead += 100; //Stop reading stuff!
                break;
        }
        free(Data);
        
        if (have_exif && have_sof) break;
        if (SectionsRead > 4) break; // Don't read too far!
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



























    