//--------------------------------------------------------------------------
// Include file for pieces of jhead project used in imgcomp.
//
// Matthias Wandel 2015
//--------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>

//--------------------------------------------------------------------------

#ifdef _WIN32
    #include <sys/utime.h>

    // Make the Microsoft Visual c 10 deprecate warnings go away.
    // The _CRT_SECURE_NO_DEPRECATE doesn't do the trick like it should.
    #define unlink _unlink
    #define chmod _chmod
    #define access _access
    #define mktemp _mktemp
#else
    #include <utime.h>
    #include <sys/types.h>
    #include <unistd.h>
    #include <errno.h>
    #include <limits.h>
#endif


typedef unsigned char uchar;

#ifndef TRUE
    #define TRUE 1
    #define FALSE 0
#endif

#define MAX_COMMENT_SIZE 16000

#ifdef _WIN32
    #define PATH_MAX _MAX_PATH
    #define SLASH '\\'
#else
    #ifndef PATH_MAX
        #define PATH_MAX 1024
    #endif
    #define SLASH '/'
#endif


extern int DumpExifMap;

#define MAX_DATE_COPIES 10

//--------------------------------------------------------------------------
// This structure stores Exif header image elements in a simple manner
// Used to store camera data as extracted from the various ways that it can be
// stored in an exif header
typedef struct {
    char  FileName     [PATH_MAX+1];

    unsigned FileSize;
    char  CameraMake   [32];
    char  CameraModel  [40];
    char  DateTime     [20];
    unsigned Height, Width;
    int   Orientation;
    int   FlashUsed;
    float FocalLength;
    float ExposureTime;
    float ApertureFNumber;
    float Distance;
    float ExposureBias;
    float DigitalZoomRatio;
    int   FocalLength35mmEquiv; // Exif 2.2 tag - usually not present.
    int   Whitebalance;
    int   MeteringMode;
    int   ExposureProgram;
    int   ExposureMode;
    int   ISOequivalent;
    int   LightSource;
    int   DistanceRange;

    float xResolution;
    float yResolution;
    int   ResolutionUnit;

//    unsigned ThumbnailOffset;          // Exif offset to thumbnail
//    unsigned ThumbnailSize;            // Size of thumbnail.
//    unsigned LargestExifOffset;        // Last exif data referenced (to check if thumbnail is at end)

//    char  ThumbnailAtEnd;              // Exif header ends with the thumbnail
                                       // (we can only modify the thumbnail if its at the end)
//    int   ThumbnailSizeOffset;
}ImageInfo_t;



#define EXIT_FAILURE  1
#define EXIT_SUCCESS  0


// prototypes for jhead.c functions
int ReadExifPart(FILE * infile);
void ErrFatal(const char * msg);
void ErrNonfatal(const char * msg, int a1, int a2);


// Prototypes for exif.c functions.
int Exif2tm(struct tm * timeptr, char * ExifTime);
void process_EXIF (unsigned char * CharBuf, unsigned int length);
void ShowImageInfo(int ShowFileInfo);
void ShowConciseImageInfo(void);
const char * ClearOrientation(void);
void PrintFormatNumber(void * ValuePtr, int Format, int ByteCount);
double ConvertAnyFormat(void * ValuePtr, int Format);
int Get16u(void * Short);
unsigned Get32u(void * Long);
int Get32s(void * Long);
void Put32u(void * Value, unsigned PutValue);
void create_EXIF(void);

//--------------------------------------------------------------------------
// Exif format descriptor stuff
extern const int BytesPerFormat[];
#define NUM_FORMATS 12

#define FMT_BYTE       1 
#define FMT_STRING     2
#define FMT_USHORT     3
#define FMT_ULONG      4
#define FMT_URATIONAL  5
#define FMT_SBYTE      6
#define FMT_UNDEFINED  7
#define FMT_SSHORT     8
#define FMT_SLONG      9
#define FMT_SRATIONAL 10
#define FMT_SINGLE    11
#define FMT_DOUBLE    12




// Variables from jhead.c used by exif.c
extern ImageInfo_t ImageInfo;
extern int ShowTags;

//--------------------------------------------------------------------------
// JPEG markers consist of one or more 0xFF bytes, followed by a marker
// code byte (which is not an FF).  Here are the marker codes of interest
// in this program.  (See jdmarker.c for a more complete list.)
//--------------------------------------------------------------------------

#define M_SOF0  0xC0          // Start Of Frame N
#define M_SOF1  0xC1          // N indicates which compression process
#define M_SOF2  0xC2          // Only SOF0-SOF2 are now in common use
#define M_SOF3  0xC3
#define M_SOF5  0xC5          // NB: codes C4 and CC are NOT SOF markers
#define M_SOF6  0xC6
#define M_SOF7  0xC7
#define M_SOF9  0xC9
#define M_SOF10 0xCA
#define M_SOF11 0xCB
#define M_SOF13 0xCD
#define M_SOF14 0xCE
#define M_SOF15 0xCF
#define M_SOI   0xD8          // Start Of Image (beginning of datastream)
#define M_EOI   0xD9          // End Of Image (end of datastream)
#define M_SOS   0xDA          // Start Of Scan (begins compressed data)
#define M_JFIF  0xE0          // Jfif marker
#define M_EXIF  0xE1          // Exif marker.  Also used for XMP data!
#define M_XMP   0x10E1        // Not a real tag (same value in file as Exif!)
#define M_COM   0xFE          // COMment 
#define M_DQT   0xDB          // Define Quantization Table
#define M_DHT   0xC4          // Define Huffmann Table
#define M_DRI   0xDD
#define M_IPTC  0xED          // IPTC marker
