//--------------------------------------------------------------------------
// Copied from jhead project to parse exif file header.
// Matthias Wandel
//
// Imgcomp is licensed under GPL v2 (see README.txt)
//--------------------------------------------------------------------------
#include "jhead.h"

#include <math.h>

static unsigned char * DirWithThumbnailPtrs;
static double FocalplaneXRes;
static double FocalplaneUnits;
static int ExifImageWidth;
int MotorolaOrder = 0;

typedef struct {
    unsigned short Tag;
    char * Desc;
}TagTable_t;


//--------------------------------------------------------------------------
// Table of Jpeg encoding process names
static const TagTable_t ProcessTable[] = {
    { M_SOF0,   "Baseline"},
    { M_SOF1,   "Extended sequential"},
    { M_SOF2,   "Progressive"},
    { M_SOF3,   "Lossless"},
    { M_SOF5,   "Differential sequential"},
    { M_SOF6,   "Differential progressive"},
    { M_SOF7,   "Differential lossless"},
    { M_SOF9,   "Extended sequential, arithmetic coding"},
    { M_SOF10,  "Progressive, arithmetic coding"},
    { M_SOF11,  "Lossless, arithmetic coding"},
    { M_SOF13,  "Differential sequential, arithmetic coding"},
    { M_SOF14,  "Differential progressive, arithmetic coding"},
    { M_SOF15,  "Differential lossless, arithmetic coding"},
};

#define PROCESS_TABLE_SIZE  (sizeof(ProcessTable) / sizeof(TagTable_t))

// 1 - "The 0th row is at the visual top of the image,    and the 0th column is the visual left-hand side."
// 2 - "The 0th row is at the visual top of the image,    and the 0th column is the visual right-hand side."
// 3 - "The 0th row is at the visual bottom of the image, and the 0th column is the visual right-hand side."
// 4 - "The 0th row is at the visual bottom of the image, and the 0th column is the visual left-hand side."

// 5 - "The 0th row is the visual left-hand side of of the image,  and the 0th column is the visual top."
// 6 - "The 0th row is the visual right-hand side of of the image, and the 0th column is the visual top."
// 7 - "The 0th row is the visual right-hand side of of the image, and the 0th column is the visual bottom."
// 8 - "The 0th row is the visual left-hand side of of the image,  and the 0th column is the visual bottom."

// Note: The descriptions here are the same as the name of the command line
// option to pass to jpegtran to right the image

static const char * OrientTab[9] = {
    "Undefined",
    "Normal",           // 1
    "flip horizontal",  // left right reversed mirror
    "rotate 180",       // 3
    "flip vertical",    // upside down mirror
    "transpose",        // Flipped about top-left <--> bottom-right axis.
    "rotate 90",        // rotate 90 cw to right it.
    "transverse",       // flipped about top-right <--> bottom-left axis
    "rotate 270",       // rotate 270 to right it.
};

const int BytesPerFormat[] = {0,1,1,2,4,8,1,1,2,4,8,4,8};

//--------------------------------------------------------------------------
// Describes tag values

#define TAG_INTEROP_INDEX          0x0001
#define TAG_INTEROP_VERSION        0x0002
#define TAG_IMAGE_WIDTH            0x0100
#define TAG_IMAGE_LENGTH           0x0101
#define TAG_BITS_PER_SAMPLE        0x0102
#define TAG_COMPRESSION            0x0103
#define TAG_PHOTOMETRIC_INTERP     0x0106
#define TAG_FILL_ORDER             0x010A
#define TAG_DOCUMENT_NAME          0x010D
#define TAG_IMAGE_DESCRIPTION      0x010E
#define TAG_MAKE                   0x010F
#define TAG_MODEL                  0x0110
#define TAG_SRIP_OFFSET            0x0111
#define TAG_ORIENTATION            0x0112
#define TAG_SAMPLES_PER_PIXEL      0x0115
#define TAG_ROWS_PER_STRIP         0x0116
#define TAG_STRIP_BYTE_COUNTS      0x0117
#define TAG_X_RESOLUTION           0x011A
#define TAG_Y_RESOLUTION           0x011B
#define TAG_PLANAR_CONFIGURATION   0x011C
#define TAG_RESOLUTION_UNIT        0x0128
#define TAG_TRANSFER_FUNCTION      0x012D
#define TAG_SOFTWARE               0x0131
#define TAG_DATETIME               0x0132
#define TAG_ARTIST                 0x013B
#define TAG_WHITE_POINT            0x013E
#define TAG_PRIMARY_CHROMATICITIES 0x013F
#define TAG_TRANSFER_RANGE         0x0156
#define TAG_JPEG_PROC              0x0200
#define TAG_THUMBNAIL_OFFSET       0x0201
#define TAG_THUMBNAIL_LENGTH       0x0202
#define TAG_Y_CB_CR_COEFFICIENTS   0x0211
#define TAG_Y_CB_CR_SUB_SAMPLING   0x0212
#define TAG_Y_CB_CR_POSITIONING    0x0213
#define TAG_REFERENCE_BLACK_WHITE  0x0214
#define TAG_RELATED_IMAGE_WIDTH    0x1001
#define TAG_RELATED_IMAGE_LENGTH   0x1002
#define TAG_CFA_REPEAT_PATTERN_DIM 0x828D
#define TAG_CFA_PATTERN1           0x828E
#define TAG_BATTERY_LEVEL          0x828F
#define TAG_COPYRIGHT              0x8298
#define TAG_EXPOSURETIME           0x829A
#define TAG_FNUMBER                0x829D
#define TAG_IPTC_NAA               0x83BB
#define TAG_EXIF_OFFSET            0x8769
#define TAG_INTER_COLOR_PROFILE    0x8773
#define TAG_EXPOSURE_PROGRAM       0x8822
#define TAG_SPECTRAL_SENSITIVITY   0x8824
#define TAG_GPSINFO                0x8825
#define TAG_ISO_EQUIVALENT         0x8827
#define TAG_OECF                   0x8828
#define TAG_EXIF_VERSION           0x9000
#define TAG_DATETIME_ORIGINAL      0x9003
#define TAG_DATETIME_DIGITIZED     0x9004
#define TAG_COMPONENTS_CONFIG      0x9101
#define TAG_CPRS_BITS_PER_PIXEL    0x9102
#define TAG_SHUTTERSPEED           0x9201
#define TAG_APERTURE               0x9202
#define TAG_BRIGHTNESS_VALUE       0x9203
#define TAG_EXPOSURE_BIAS          0x9204
#define TAG_MAXAPERTURE            0x9205
#define TAG_SUBJECT_DISTANCE       0x9206
#define TAG_METERING_MODE          0x9207
#define TAG_LIGHT_SOURCE           0x9208
#define TAG_FLASH                  0x9209
#define TAG_FOCALLENGTH            0x920A
#define TAG_SUBJECTAREA            0x9214
#define TAG_MAKER_NOTE             0x927C
#define TAG_USERCOMMENT            0x9286
#define TAG_SUBSEC_TIME            0x9290
#define TAG_SUBSEC_TIME_ORIG       0x9291
#define TAG_SUBSEC_TIME_DIG        0x9292

#define TAG_WINXP_TITLE            0x9c9b // Windows XP - not part of exif standard.
#define TAG_WINXP_COMMENT          0x9c9c // Windows XP - not part of exif standard.
#define TAG_WINXP_AUTHOR           0x9c9d // Windows XP - not part of exif standard.
#define TAG_WINXP_KEYWORDS         0x9c9e // Windows XP - not part of exif standard.
#define TAG_WINXP_SUBJECT          0x9c9f // Windows XP - not part of exif standard.

#define TAG_FLASH_PIX_VERSION      0xA000
#define TAG_COLOR_SPACE            0xA001
#define TAG_PIXEL_X_DIMENSION      0xA002
#define TAG_PIXEL_Y_DIMENSION      0xA003
#define TAG_RELATED_AUDIO_FILE     0xA004
#define TAG_INTEROP_OFFSET         0xA005
#define TAG_FLASH_ENERGY           0xA20B
#define TAG_SPATIAL_FREQ_RESP      0xA20C
#define TAG_FOCAL_PLANE_XRES       0xA20E
#define TAG_FOCAL_PLANE_YRES       0xA20F
#define TAG_FOCAL_PLANE_UNITS      0xA210
#define TAG_SUBJECT_LOCATION       0xA214
#define TAG_EXPOSURE_INDEX         0xA215
#define TAG_SENSING_METHOD         0xA217
#define TAG_FILE_SOURCE            0xA300
#define TAG_SCENE_TYPE             0xA301
#define TAG_CFA_PATTERN            0xA302
#define TAG_CUSTOM_RENDERED        0xA401
#define TAG_EXPOSURE_MODE          0xA402
#define TAG_WHITEBALANCE           0xA403
#define TAG_DIGITALZOOMRATIO       0xA404
#define TAG_FOCALLENGTH_35MM       0xA405
#define TAG_SCENE_CAPTURE_TYPE     0xA406
#define TAG_GAIN_CONTROL           0xA407
#define TAG_CONTRAST               0xA408
#define TAG_SATURATION             0xA409
#define TAG_SHARPNESS              0xA40A
#define TAG_DISTANCE_RANGE         0xA40C
#define TAG_IMAGE_UNIQUE_ID        0xA420

static const TagTable_t TagTable[] = {
  { TAG_INTEROP_INDEX,          "InteropIndex"},
  { TAG_INTEROP_VERSION,        "InteropVersion"},
  { TAG_IMAGE_WIDTH,            "ImageWidth"},
  { TAG_IMAGE_LENGTH,           "ImageLength"},
  { TAG_BITS_PER_SAMPLE,        "BitsPerSample"},
  { TAG_COMPRESSION,            "Compression"},
  { TAG_PHOTOMETRIC_INTERP,     "PhotometricInterpretation"},
  { TAG_FILL_ORDER,             "FillOrder"},
  { TAG_DOCUMENT_NAME,          "DocumentName"},
  { TAG_IMAGE_DESCRIPTION,      "ImageDescription"},
  { TAG_MAKE,                   "Make"},
  { TAG_MODEL,                  "Model"},
  { TAG_SRIP_OFFSET,            "StripOffsets"},
  { TAG_ORIENTATION,            "Orientation"},
  { TAG_SAMPLES_PER_PIXEL,      "SamplesPerPixel"},
  { TAG_ROWS_PER_STRIP,         "RowsPerStrip"},
  { TAG_STRIP_BYTE_COUNTS,      "StripByteCounts"},
  { TAG_X_RESOLUTION,           "XResolution"},
  { TAG_Y_RESOLUTION,           "YResolution"},
  { TAG_PLANAR_CONFIGURATION,   "PlanarConfiguration"},
  { TAG_RESOLUTION_UNIT,        "ResolutionUnit"},
  { TAG_TRANSFER_FUNCTION,      "TransferFunction"},
  { TAG_SOFTWARE,               "Software"},
  { TAG_DATETIME,               "DateTime"},
  { TAG_ARTIST,                 "Artist"},
  { TAG_WHITE_POINT,            "WhitePoint"},
  { TAG_PRIMARY_CHROMATICITIES, "PrimaryChromaticities"},
  { TAG_TRANSFER_RANGE,         "TransferRange"},
  { TAG_JPEG_PROC,              "JPEGProc"},
  { TAG_THUMBNAIL_OFFSET,       "ThumbnailOffset"},
  { TAG_THUMBNAIL_LENGTH,       "ThumbnailLength"},
  { TAG_Y_CB_CR_COEFFICIENTS,   "YCbCrCoefficients"},
  { TAG_Y_CB_CR_SUB_SAMPLING,   "YCbCrSubSampling"},
  { TAG_Y_CB_CR_POSITIONING,    "YCbCrPositioning"},
  { TAG_REFERENCE_BLACK_WHITE,  "ReferenceBlackWhite"},
  { TAG_RELATED_IMAGE_WIDTH,    "RelatedImageWidth"},
  { TAG_RELATED_IMAGE_LENGTH,   "RelatedImageLength"},
  { TAG_CFA_REPEAT_PATTERN_DIM, "CFARepeatPatternDim"},
  { TAG_CFA_PATTERN1,           "CFAPattern"},
  { TAG_BATTERY_LEVEL,          "BatteryLevel"},
  { TAG_COPYRIGHT,              "Copyright"},
  { TAG_EXPOSURETIME,           "ExposureTime"},
  { TAG_FNUMBER,                "FNumber"},
  { TAG_IPTC_NAA,               "IPTC/NAA"},
  { TAG_EXIF_OFFSET,            "ExifOffset"},
  { TAG_INTER_COLOR_PROFILE,    "InterColorProfile"},
  { TAG_EXPOSURE_PROGRAM,       "ExposureProgram"},
  { TAG_SPECTRAL_SENSITIVITY,   "SpectralSensitivity"},
  { TAG_GPSINFO,                "GPS Dir offset"},
  { TAG_ISO_EQUIVALENT,         "ISOSpeedRatings"},
  { TAG_OECF,                   "OECF"},
  { TAG_EXIF_VERSION,           "ExifVersion"},
  { TAG_DATETIME_ORIGINAL,      "DateTimeOriginal"},
  { TAG_DATETIME_DIGITIZED,     "DateTimeDigitized"},
  { TAG_COMPONENTS_CONFIG,      "ComponentsConfiguration"},
  { TAG_CPRS_BITS_PER_PIXEL,    "CompressedBitsPerPixel"},
  { TAG_SHUTTERSPEED,           "ShutterSpeedValue"},
  { TAG_APERTURE,               "ApertureValue"},
  { TAG_BRIGHTNESS_VALUE,       "BrightnessValue"},
  { TAG_EXPOSURE_BIAS,          "ExposureBiasValue"},
  { TAG_MAXAPERTURE,            "MaxApertureValue"},
  { TAG_SUBJECT_DISTANCE,       "SubjectDistance"},
  { TAG_METERING_MODE,          "MeteringMode"},
  { TAG_LIGHT_SOURCE,           "LightSource"},
  { TAG_FLASH,                  "Flash"},
  { TAG_FOCALLENGTH,            "FocalLength"},
  { TAG_MAKER_NOTE,             "MakerNote"},
  { TAG_USERCOMMENT,            "UserComment"},
  { TAG_SUBSEC_TIME,            "SubSecTime"},
  { TAG_SUBSEC_TIME_ORIG,       "SubSecTimeOriginal"},
  { TAG_SUBSEC_TIME_DIG,        "SubSecTimeDigitized"},
  { TAG_WINXP_TITLE,            "Windows-XP Title"},
  { TAG_WINXP_COMMENT,          "Windows-XP comment"},
  { TAG_WINXP_AUTHOR,           "Windows-XP author"},
  { TAG_WINXP_KEYWORDS,         "Windows-XP keywords"},
  { TAG_WINXP_SUBJECT,          "Windows-XP subject"},
  { TAG_FLASH_PIX_VERSION,      "FlashPixVersion"},
  { TAG_COLOR_SPACE,            "ColorSpace"},
  { TAG_PIXEL_X_DIMENSION,      "ExifImageWidth"},
  { TAG_PIXEL_Y_DIMENSION,      "ExifImageLength"},
  { TAG_RELATED_AUDIO_FILE,     "RelatedAudioFile"},
  { TAG_INTEROP_OFFSET,         "InteroperabilityOffset"},
  { TAG_FLASH_ENERGY,           "FlashEnergy"},              
  { TAG_SPATIAL_FREQ_RESP,      "SpatialFrequencyResponse"}, 
  { TAG_FOCAL_PLANE_XRES,       "FocalPlaneXResolution"},    
  { TAG_FOCAL_PLANE_YRES,       "FocalPlaneYResolution"},    
  { TAG_FOCAL_PLANE_UNITS,      "FocalPlaneResolutionUnit"}, 
  { TAG_SUBJECT_LOCATION,       "SubjectLocation"},          
  { TAG_EXPOSURE_INDEX,         "ExposureIndex"},            
  { TAG_SENSING_METHOD,         "SensingMethod"},            
  { TAG_FILE_SOURCE,            "FileSource"},
  { TAG_SCENE_TYPE,             "SceneType"},
  { TAG_CFA_PATTERN,            "CFA Pattern"},
  { TAG_CUSTOM_RENDERED,        "CustomRendered"},
  { TAG_EXPOSURE_MODE,          "ExposureMode"},
  { TAG_WHITEBALANCE,           "WhiteBalance"},
  { TAG_DIGITALZOOMRATIO,       "DigitalZoomRatio"},
  { TAG_FOCALLENGTH_35MM,       "FocalLengthIn35mmFilm"},
  { TAG_SUBJECTAREA,            "SubjectArea"},
  { TAG_SCENE_CAPTURE_TYPE,     "SceneCaptureType"},
  { TAG_GAIN_CONTROL,           "GainControl"},
  { TAG_CONTRAST,               "Contrast"},
  { TAG_SATURATION,             "Saturation"},
  { TAG_SHARPNESS,              "Sharpness"},
  { TAG_DISTANCE_RANGE,         "SubjectDistanceRange"},
  { TAG_IMAGE_UNIQUE_ID,        "ImageUniqueId"},
} ;

#define TAG_TABLE_SIZE  (sizeof(TagTable) / sizeof(TagTable_t))

//--------------------------------------------------------------------------
// Convert a 16 bit unsigned value from file's native byte order
//--------------------------------------------------------------------------
int Get16u(void * Short)
{
    if (MotorolaOrder){
        return (((uchar *)Short)[0] << 8) | ((uchar *)Short)[1];
    }else{
        return (((uchar *)Short)[1] << 8) | ((uchar *)Short)[0];
    }
}

//--------------------------------------------------------------------------
// Convert a 32 bit signed value from file's native byte order
//--------------------------------------------------------------------------
int Get32s(void * Long)
{
    if (MotorolaOrder){
        return  ((( char *)Long)[0] << 24) | (((uchar *)Long)[1] << 16)
              | (((uchar *)Long)[2] << 8 ) | (((uchar *)Long)[3] << 0 );
    }else{
        return  ((( char *)Long)[3] << 24) | (((uchar *)Long)[2] << 16)
              | (((uchar *)Long)[1] << 8 ) | (((uchar *)Long)[0] << 0 );
    }
}

//--------------------------------------------------------------------------
// Convert a 32 bit unsigned value to file's native byte order
//--------------------------------------------------------------------------
void Put32u(void * Value, unsigned PutValue)
{
    if (MotorolaOrder){
        ((uchar *)Value)[0] = (uchar)(PutValue>>24);
        ((uchar *)Value)[1] = (uchar)(PutValue>>16);
        ((uchar *)Value)[2] = (uchar)(PutValue>>8);
        ((uchar *)Value)[3] = (uchar)PutValue;
    }else{
        ((uchar *)Value)[0] = (uchar)PutValue;
        ((uchar *)Value)[1] = (uchar)(PutValue>>8);
        ((uchar *)Value)[2] = (uchar)(PutValue>>16);
        ((uchar *)Value)[3] = (uchar)(PutValue>>24);
    }
}

//--------------------------------------------------------------------------
// Convert a 32 bit unsigned value from file's native byte order
//--------------------------------------------------------------------------
unsigned Get32u(void * Long)
{
    return (unsigned)Get32s(Long) & 0xffffffff;
}

//--------------------------------------------------------------------------
// Display a number as one of its many formats
//--------------------------------------------------------------------------
void PrintFormatNumber(void * ValuePtr, int Format, int ByteCount)
{
    int s,n;

    for(n=0;n<16;n++){
        switch(Format){
            case FMT_SBYTE:
            case FMT_BYTE:      printf("%02x",*(uchar *)ValuePtr); s=1;  break;
            case FMT_USHORT:    printf("%d",Get16u(ValuePtr)); s=2;      break;
            case FMT_ULONG:     
            case FMT_SLONG:     printf("%d",Get32s(ValuePtr)); s=4;      break;
            case FMT_SSHORT:    printf("%hd",(signed short)Get16u(ValuePtr)); s=2; break;
            case FMT_URATIONAL:
                printf("%u/%u",Get32s(ValuePtr), Get32s(4+(char *)ValuePtr)); 
                s = 8;
                break;

            case FMT_SRATIONAL: 
                printf("%d/%d",Get32s(ValuePtr), Get32s(4+(char *)ValuePtr)); 
                s = 8;
                break;

            case FMT_SINGLE:    printf("%f",(double)*(float *)ValuePtr); s=8; break;
            case FMT_DOUBLE:    printf("%f",*(double *)ValuePtr);        s=8; break;
            default: 
                printf("Unknown format %d:", Format);
                return;
        }
        ByteCount -= s;
        if (ByteCount <= 0) break;
        printf(", ");
        ValuePtr = (void *)((char *)ValuePtr + s);

    }
    if (n >= 16) printf("...");
}


//--------------------------------------------------------------------------
// Evaluate number, be it int, rational, or float from directory.
//--------------------------------------------------------------------------
double ConvertAnyFormat(void * ValuePtr, int Format)
{
    double Value;
    Value = 0;

    switch(Format){
        case FMT_SBYTE:     Value = *(signed char *)ValuePtr;  break;
        case FMT_BYTE:      Value = *(uchar *)ValuePtr;        break;

        case FMT_USHORT:    Value = Get16u(ValuePtr);          break;
        case FMT_ULONG:     Value = Get32u(ValuePtr);          break;

        case FMT_URATIONAL:
        case FMT_SRATIONAL: 
            {
                int Num,Den;
                Num = Get32s(ValuePtr);
                Den = Get32s(4+(char *)ValuePtr);
                if (Den == 0){
                    Value = 0;
                }else{
                    if (Format == FMT_SRATIONAL){
                        Value = (double)Num/Den;
                    }else{
                        Value = (double)(unsigned)Num/(double)(unsigned)Den;
                    }
                }
                break;
            }

        case FMT_SSHORT:    Value = (signed short)Get16u(ValuePtr);  break;
        case FMT_SLONG:     Value = Get32s(ValuePtr);                break;

        // Not sure if this is correct (never seen float used in Exif format)
        case FMT_SINGLE:    Value = (double)*(float *)ValuePtr;      break;
        case FMT_DOUBLE:    Value = *(double *)ValuePtr;             break;

        default:
            ErrNonfatal("Illegal format code %d in Exif header",Format,0);
    }
    return Value;
}

//--------------------------------------------------------------------------
// Process one of the nested EXIF directories.
//--------------------------------------------------------------------------
static void ProcessExifDir(unsigned char * DirStart, unsigned char * OffsetBase, 
        unsigned ExifLength, int NestingLevel)
{
    int de;
    int a;
    int NumDirEntries;
    char IndentString[25];

    if (NestingLevel > 4){
        ErrNonfatal("Maximum Exif directory nesting exceeded (corrupt Exif header)", 0,0);
        return;
    }

    memset(IndentString, ' ', 25);
    IndentString[NestingLevel * 4] = '\0';


    NumDirEntries = Get16u(DirStart);
    #define DIR_ENTRY_ADDR(Start, Entry) (Start+2+12*(Entry))

    {
        unsigned char * DirEnd;
        DirEnd = DIR_ENTRY_ADDR(DirStart, NumDirEntries);
        if (DirEnd+4 > (OffsetBase+ExifLength)){
            if (DirEnd+2 == OffsetBase+ExifLength || DirEnd == OffsetBase+ExifLength){
                // Version 1.3 of jhead would truncate a bit too much.
                // This also caught later on as well.
            }else{
                ErrNonfatal("Illegally sized Exif subdirectory (%d entries)",NumDirEntries,0);
                return;
            }
        }
    }

    if (ShowTags){
        printf("(dir has %d entries)\n",NumDirEntries);
    }

    for (de=0;de<NumDirEntries;de++){
        int Tag, Format, Components;
        unsigned char * ValuePtr;
        int ByteCount;
        unsigned char * DirEntry;
        DirEntry = DIR_ENTRY_ADDR(DirStart, de);

        Tag = Get16u(DirEntry);
        Format = Get16u(DirEntry+2);
        Components = Get32u(DirEntry+4);

        if ((Format-1) >= NUM_FORMATS) {
            // (-1) catches illegal zero case as unsigned underflows to positive large.
            ErrNonfatal("Illegal number format %d for tag %04x in Exif", Format, Tag);
            continue;
        }

        if ((unsigned)Components > 0x10000){
            ErrNonfatal("Too many components %d for tag %04x in Exif", Components, Tag);
            continue;
        }

        ByteCount = Components * BytesPerFormat[Format];

        if (ByteCount > 4){
            unsigned OffsetVal;
            OffsetVal = Get32u(DirEntry+8);
            // If its bigger than 4 bytes, the dir entry contains an offset.
            if (OffsetVal+ByteCount > ExifLength){
                // Bogus pointer offset and / or bytecount value
                ErrNonfatal("Illegal value pointer for tag %04x in Exif", Tag,0);
                continue;
            }
            ValuePtr = OffsetBase+OffsetVal;
        }else{
            // 4 bytes or less and value is in the dir entry itself
            ValuePtr = DirEntry+8;
        }

        if (ShowTags){
            // Show tag name
            for (a=0;;a++){
                if (a >= TAG_TABLE_SIZE){
                    printf("%s    Unknown Tag %04x Value = ", IndentString, Tag);
                    break;
                }
                if (TagTable[a].Tag == Tag){
                    printf("%s    %s = ",IndentString, TagTable[a].Desc);
                    break;
                }
            }

            // Show tag value.
            switch(Format){
                case FMT_BYTE:
                    if(ByteCount>1){
                        printf("%.*ls\n", ByteCount/2, (wchar_t *)ValuePtr);
                    }else{
                        PrintFormatNumber(ValuePtr, Format, ByteCount);
                        printf("\n");
                    }
                    break;

                case FMT_UNDEFINED:
                    // Undefined is typically an ascii string.

                case FMT_STRING:
                    // String arrays printed without function call (different from int arrays)
                    {
                        int NoPrint = 0;
                        printf("\"");
                        for (a=0;a<ByteCount;a++){
                            if (ValuePtr[a] >= 32){
                                putchar(ValuePtr[a]);
                                NoPrint = 0;
                            }else{
                                // Avoiding indicating too many unprintable characters of proprietary
                                // bits of binary information this program may not know how to parse.
                                if (!NoPrint && a != ByteCount-1){
                                    putchar('?');
                                    NoPrint = 1;
                                }
                            }
                        }
                        printf("\"\n");
                    }
                    break;

                default:
                    // Handle arrays of numbers later (will there ever be?)
                    PrintFormatNumber(ValuePtr, Format, ByteCount);
                    printf("\n");
            }
        }

        // Extract useful components of tag
        switch(Tag){

            case TAG_MAKE:
                strncpy(ImageInfo.CameraMake, (char *)ValuePtr, ByteCount < 31 ? ByteCount : 31);
                break;

            case TAG_MODEL:
                strncpy(ImageInfo.CameraModel, (char *)ValuePtr, ByteCount < 39 ? ByteCount : 39);
                break;

            case TAG_DATETIME_ORIGINAL:
                // If we get a DATETIME_ORIGINAL, we use that one.
                strncpy(ImageInfo.DateTime, (char *)ValuePtr, 19);
                // Fallthru...

            case TAG_DATETIME_DIGITIZED:
            case TAG_DATETIME:
                if (!isdigit(ImageInfo.DateTime[0])){
                    // If we don't already have a DATETIME_ORIGINAL, use whatever
                    // time fields we may have.
                    strncpy(ImageInfo.DateTime, (char *)ValuePtr, 19);
                }
                break;

            case TAG_FNUMBER:
                // Simplest way of expressing aperture, so I trust it the most.
                // (overwrite previously computd value if there is one)
                ImageInfo.ApertureFNumber = (float)ConvertAnyFormat(ValuePtr, Format);
                break;

#if 0 // don't pull in extra math stuff.
            case TAG_APERTURE:
            case TAG_MAXAPERTURE:
                // More relevant info always comes earlier, so only use this field if we don't 
                // have appropriate aperture information yet.
                if (ImageInfo.ApertureFNumber == 0){
                    ImageInfo.ApertureFNumber 
                        = (float)exp(ConvertAnyFormat(ValuePtr, Format)*log(2)*0.5);
                }
                break;

            case TAG_SHUTTERSPEED:
                // More complicated way of expressing exposure time, so only use
                // this value if we don't already have it from somewhere else.
                if (ImageInfo.ExposureTime == 0){
                    ImageInfo.ExposureTime 
                        = (float)(1/exp(ConvertAnyFormat(ValuePtr, Format)*log(2)));
                }
                break;
#endif
            case TAG_FOCALLENGTH:
                // Nice digital cameras actually save the focal length as a function
                // of how farthey are zoomed in.
                ImageInfo.FocalLength = (float)ConvertAnyFormat(ValuePtr, Format);
                break;

            case TAG_SUBJECT_DISTANCE:
                // Inidcates the distacne the autofocus camera is focused to.
                // Tends to be less accurate as distance increases.
                ImageInfo.Distance = (float)ConvertAnyFormat(ValuePtr, Format);
                break;

            case TAG_EXPOSURETIME:
                // Simplest way of expressing exposure time, so I trust it most.
                // (overwrite previously computd value if there is one)
                ImageInfo.ExposureTime = (float)ConvertAnyFormat(ValuePtr, Format);
                break;



            case TAG_FLASH:
                ImageInfo.FlashUsed=(int)ConvertAnyFormat(ValuePtr, Format);
                break;

            case TAG_ORIENTATION:
                ImageInfo.Orientation = (int)ConvertAnyFormat(ValuePtr, Format);
                break;

            case TAG_PIXEL_Y_DIMENSION:
            case TAG_PIXEL_X_DIMENSION:
                // Use largest of height and width to deal with images that have been
                // rotated to portrait format.
                a = (int)ConvertAnyFormat(ValuePtr, Format);
                if (ExifImageWidth < a) ExifImageWidth = a;
                break;

            case TAG_FOCAL_PLANE_XRES:
                FocalplaneXRes = ConvertAnyFormat(ValuePtr, Format);
                break;

            case TAG_FOCAL_PLANE_UNITS:
                switch((int)ConvertAnyFormat(ValuePtr, Format)){
                    case 1: FocalplaneUnits = 25.4; break; // inch
                    case 2: 
                        // According to the information I was using, 2 means meters.
                        // But looking at the Cannon powershot's files, inches is the only
                        // sensible value.
                        FocalplaneUnits = 25.4;
                        break;

                    case 3: FocalplaneUnits = 10;   break;  // centimeter
                    case 4: FocalplaneUnits = 1;    break;  // millimeter
                    case 5: FocalplaneUnits = .001; break;  // micrometer
                }
                break;

            case TAG_EXPOSURE_BIAS:
                ImageInfo.ExposureBias = (float)ConvertAnyFormat(ValuePtr, Format);
                break;

            case TAG_WHITEBALANCE:
                ImageInfo.Whitebalance = (int)ConvertAnyFormat(ValuePtr, Format);
                break;

            case TAG_LIGHT_SOURCE:
                ImageInfo.LightSource = (int)ConvertAnyFormat(ValuePtr, Format);
                break;

            case TAG_METERING_MODE:
                ImageInfo.MeteringMode = (int)ConvertAnyFormat(ValuePtr, Format);
                break;

            case TAG_EXPOSURE_PROGRAM:
                ImageInfo.ExposureProgram = (int)ConvertAnyFormat(ValuePtr, Format);
                break;

            case TAG_EXPOSURE_INDEX:
                if (ImageInfo.ISOequivalent == 0){
                    // Exposure index and ISO equivalent are often used interchangeably,
                    // so we will do the same in jhead.
                    // http://photography.about.com/library/glossary/bldef_ei.htm
                    ImageInfo.ISOequivalent = (int)ConvertAnyFormat(ValuePtr, Format);
                }
                break;

            case TAG_EXPOSURE_MODE:
                ImageInfo.ExposureMode = (int)ConvertAnyFormat(ValuePtr, Format);
                break;

            case TAG_ISO_EQUIVALENT:
                ImageInfo.ISOequivalent = (int)ConvertAnyFormat(ValuePtr, Format);
                break;

            case TAG_DIGITALZOOMRATIO:
                ImageInfo.DigitalZoomRatio = (float)ConvertAnyFormat(ValuePtr, Format);
                break;

            case TAG_EXIF_OFFSET:
                if (ShowTags) printf("%s    Exif Dir:",IndentString);

            case TAG_INTEROP_OFFSET:
                if (Tag == TAG_INTEROP_OFFSET && ShowTags) printf("%s    Interop Dir:",IndentString);
                {
                    unsigned char * SubdirStart;
                    SubdirStart = OffsetBase + Get32u(ValuePtr);
                    if (SubdirStart < OffsetBase || SubdirStart > OffsetBase+ExifLength){
                        ErrNonfatal("Illegal Exif or interop ofset directory link",0,0);
                    }else{
                        ProcessExifDir(SubdirStart, OffsetBase, ExifLength, NestingLevel+1);
                    }
                    continue;
                }
                break;

            case TAG_FOCALLENGTH_35MM:
                // The focal length equivalent 35 mm is a 2.2 tag (defined as of April 2002)
                // if its present, use it to compute equivalent focal length instead of 
                // computing it from sensor geometry and actual focal length.
                ImageInfo.FocalLength35mmEquiv = (unsigned)ConvertAnyFormat(ValuePtr, Format);
                break;

            case TAG_DISTANCE_RANGE:
                // Three possible standard values:
                //   1 = macro, 2 = close, 3 = distant
                ImageInfo.DistanceRange = (int)ConvertAnyFormat(ValuePtr, Format);
                break;



            case TAG_X_RESOLUTION:
                if (NestingLevel==0) {// Only use the values from the top level directory
                    ImageInfo.xResolution = (float)ConvertAnyFormat(ValuePtr,Format);
                    // if yResolution has not been set, use the value of xResolution
                    if (ImageInfo.yResolution == 0.0) ImageInfo.yResolution = ImageInfo.xResolution;
                }
                break;

            case TAG_Y_RESOLUTION:
                if (NestingLevel==0) {// Only use the values from the top level directory
                    ImageInfo.yResolution = (float)ConvertAnyFormat(ValuePtr,Format);
                    // if xResolution has not been set, use the value of yResolution
                    if (ImageInfo.xResolution == 0.0) ImageInfo.xResolution = ImageInfo.yResolution;
                }
                break;

            case TAG_RESOLUTION_UNIT:
                if (NestingLevel==0) {// Only use the values from the top level directory
                    ImageInfo.ResolutionUnit = (int) ConvertAnyFormat(ValuePtr,Format);
                }
                break;

        }
    }


    {
        // In addition to linking to subdirectories via exif tags, 
        // there's also a potential link to another directory at the end of each
        // directory.  this has got to be the result of a committee!
        unsigned char * SubdirStart;
        unsigned Offset;

        if (DIR_ENTRY_ADDR(DirStart, NumDirEntries) + 4 <= OffsetBase+ExifLength){
            Offset = Get32u(DirStart+2+12*NumDirEntries);
            if (Offset){
                SubdirStart = OffsetBase + Offset;
                if (SubdirStart > OffsetBase+ExifLength || SubdirStart < OffsetBase){
                    if (SubdirStart > OffsetBase && SubdirStart < OffsetBase+ExifLength+20){
                        // Jhead 1.3 or earlier would crop the whole directory!
                        // As Jhead produces this form of format incorrectness, 
                        // I'll just let it pass silently
                        if (ShowTags) printf("Thumbnail removed with Jhead 1.3 or earlier\n");
                    }else{
                        ErrNonfatal("Illegal subdirectory link in Exif header",0,0);
                    }
                }else{
                    if (SubdirStart <= OffsetBase+ExifLength){
                        if (ShowTags) printf("%s    Continued directory ",IndentString);
                        ProcessExifDir(SubdirStart, OffsetBase, ExifLength, NestingLevel+1);
                    }
                }
            }
        }else{
            // The exif header ends before the last next directory pointer.
        }
    }
}


//--------------------------------------------------------------------------
// Process a EXIF marker
// Describes all the drivel that most digital cameras include...
//--------------------------------------------------------------------------
void process_EXIF (unsigned char * ExifSection, unsigned int length)
{
    unsigned int FirstOffset;

    FocalplaneXRes = 0;
    FocalplaneUnits = 0;
    ExifImageWidth = 0;

    if (ShowTags){
        printf("Exif header %u bytes long\n",length);
    }

    {   // Check the EXIF header component
        static uchar ExifHeader[] = "Exif\0\0";
        if (memcmp(ExifSection+2, ExifHeader,6)){
            ErrNonfatal("Incorrect Exif header",0,0);
            return;
        }
    }

    if (memcmp(ExifSection+8,"II",2) == 0){
        if (ShowTags) printf("Exif section in Intel order\n");
        MotorolaOrder = 0;
    }else{
        if (memcmp(ExifSection+8,"MM",2) == 0){
            if (ShowTags) printf("Exif section in Motorola order\n");
            MotorolaOrder = 1;
        }else{
            ErrNonfatal("Invalid Exif alignment marker.",0,0);
            return;
        }
    }

    // Check the next value for correctness.
    if (Get16u(ExifSection+10) != 0x2a){
        ErrNonfatal("Invalid Exif start (1)",0,0);
        return;
    }

    FirstOffset = Get32u(ExifSection+12);
    if (FirstOffset < 8 || FirstOffset > 16){
        if (FirstOffset < 16 || FirstOffset > length-16){
            ErrNonfatal("invalid offset for first Exif IFD value",0,0);
            return;
        }
        // Usually set to 8, but other values valid too.
        ErrNonfatal("Suspicious offset of first Exif IFD value",0,0);
    }

    DirWithThumbnailPtrs = NULL;

    // First directory starts 16 bytes in.  All offset are relative to 8 bytes in.
    ProcessExifDir(ExifSection+8+FirstOffset, ExifSection+8, length-8, 0);
}




//--------------------------------------------------------------------------
// Convert exif time to Unix time structure
//--------------------------------------------------------------------------
int Exif2tm(struct tm * timeptr, char * ExifTime)
{
    int a;

    timeptr->tm_wday = -1;

    // Check for format: YYYY:MM:DD HH:MM:SS format.
    // Date and time normally separated by a space, but also seen a ':' there, so
    // skip the middle space with '%*c' so it can be any character.
    timeptr->tm_sec = 0;
    a = sscanf(ExifTime, "%d%*c%d%*c%d%*c%d:%d:%d",
            &timeptr->tm_year, &timeptr->tm_mon, &timeptr->tm_mday,
            &timeptr->tm_hour, &timeptr->tm_min, &timeptr->tm_sec);

    if (a >= 5){
        if (timeptr->tm_year <= 12 && timeptr->tm_mday > 2000 && ExifTime[2] == '.'){
            // LG Electronics VX-9700 seems to encode the date as 'MM.DD.YYYY HH:MM'
            // can't these people read the standard?  At least they left enough room
            // in the header to put an Exif format date in there.
            int tmp;
            tmp = timeptr->tm_year;
            timeptr->tm_year = timeptr->tm_mday;
            timeptr->tm_mday = timeptr->tm_mon;
            timeptr->tm_mon = tmp;
        }

        // Accept five or six parameters.  Some cameras do not store seconds.
        timeptr->tm_isdst = -1;  
        timeptr->tm_mon -= 1;      // Adjust for unix zero-based months 
        timeptr->tm_year -= 1900;  // Adjust for year starting at 1900 
        return TRUE; // worked. 
    }

    return FALSE; // Wasn't in Exif date format.
}



//--------------------------------------------------------------------------
// Show the collected image info, displaying camera F-stop and shutter speed
// in a consistent and legible fashion.
//--------------------------------------------------------------------------
void ShowImageInfo(int ShowFileInfo)
{
    if (ShowFileInfo){
        printf("File name    : %s\n",ImageInfo.FileName);
        printf("File size    : %d bytes\n",ImageInfo.FileSize);
    }

    if (ImageInfo.CameraMake[0]){
        printf("Camera make  : %s\n",ImageInfo.CameraMake);
        printf("Camera model : %s\n",ImageInfo.CameraModel);
    }
    if (ImageInfo.DateTime[0]){
        printf("Date/Time    : %s\n",ImageInfo.DateTime);
    }
    printf("Resolution   : %d x %d\n",ImageInfo.Width, ImageInfo.Height);

    if (ImageInfo.Orientation > 1 && ImageInfo.Orientation <=8){
        // Only print orientation if one was supplied, and if its not 1 (normal orientation)
        printf("Orientation  : %s\n", OrientTab[ImageInfo.Orientation]);
    }

    if (ImageInfo.FlashUsed >= 0){
        if (ImageInfo.FlashUsed & 1){    
            printf("Flash used   : Yes");
            switch (ImageInfo.FlashUsed){
	            case 0x5: printf(" (Strobe light not detected)"); break;
	            case 0x7: printf(" (Strobe light detected) "); break;
	            case 0x9: printf(" (manual)"); break;
	            case 0xd: printf(" (manual, return light not detected)"); break;
	            case 0xf: printf(" (manual, return light  detected)"); break;
	            case 0x19:printf(" (auto)"); break;
	            case 0x1d:printf(" (auto, return light not detected)"); break;
	            case 0x1f:printf(" (auto, return light detected)"); break;
	            case 0x41:printf(" (red eye reduction mode)"); break;
	            case 0x45:printf(" (red eye reduction mode return light not detected)"); break;
	            case 0x47:printf(" (red eye reduction mode return light  detected)"); break;
	            case 0x49:printf(" (manual, red eye reduction mode)"); break;
	            case 0x4d:printf(" (manual, red eye reduction mode, return light not detected)"); break;
	            case 0x4f:printf(" (red eye reduction mode, return light detected)"); break;
	            case 0x59:printf(" (auto, red eye reduction mode)"); break;
	            case 0x5d:printf(" (auto, red eye reduction mode, return light not detected)"); break;
	            case 0x5f:printf(" (auto, red eye reduction mode, return light detected)"); break;
            }
        }else{
            printf("Flash used   : No");
            switch (ImageInfo.FlashUsed){
	            case 0x18:printf(" (auto)"); break;
            }
        }
        printf("\n");
    }


    if (ImageInfo.FocalLength){
        printf("Focal length : %4.1fmm",(double)ImageInfo.FocalLength);
        if (ImageInfo.FocalLength35mmEquiv){
            printf("  (35mm equivalent: %dmm)", ImageInfo.FocalLength35mmEquiv);
        }
        printf("\n");
    }

    if (ImageInfo.DigitalZoomRatio > 1){
        // Digital zoom used.  Shame on you!
        printf("Digital Zoom : %1.3fx\n", (double)ImageInfo.DigitalZoomRatio);
    }

    if (ImageInfo.ExposureTime){
        if (ImageInfo.ExposureTime < 0.010){
            printf("Exposure time: %6.4f s ",(double)ImageInfo.ExposureTime);
        }else{
            printf("Exposure time: %5.3f s ",(double)ImageInfo.ExposureTime);
        }
        if (ImageInfo.ExposureTime <= 0.5){
            printf(" (1/%d)",(int)(0.5 + 1/ImageInfo.ExposureTime));
        }
        printf("\n");
    }
    if (ImageInfo.ApertureFNumber){
        printf("Aperture     : f/%3.1f\n",(double)ImageInfo.ApertureFNumber);
    }
    if (ImageInfo.Distance){
        if (ImageInfo.Distance < 0){
            printf("Focus dist.  : Infinite\n");
        }else{
            printf("Focus dist.  : %4.2fm\n",(double)ImageInfo.Distance);
        }
    }

    if (ImageInfo.ISOequivalent){
        printf("ISO equiv.   : %2d\n",(int)ImageInfo.ISOequivalent);
    }

    if (ImageInfo.ExposureBias){
        // If exposure bias was specified, but set to zero, presumably its no bias at all,
        // so only show it if its nonzero.
        printf("Exposure bias: %4.2f\n",(double)ImageInfo.ExposureBias);
    }
        
    switch(ImageInfo.Whitebalance) {
        case 1:
            printf("Whitebalance : Manual\n");
            break;
        case 0:
            printf("Whitebalance : Auto\n");
            break;
    }

    //Quercus: 17-1-2004 Added LightSource, some cams return this, whitebalance or both
    switch(ImageInfo.LightSource) {
        case 1:
            printf("Light Source : Daylight\n");
            break;
        case 2:
            printf("Light Source : Fluorescent\n");
            break;
        case 3:
            printf("Light Source : Incandescent\n");
            break;
        case 4:
            printf("Light Source : Flash\n");
            break;
        case 9:
            printf("Light Source : Fine weather\n");
            break;
        case 11:
            printf("Light Source : Shade\n");
            break;
        default:; //Quercus: 17-1-2004 There are many more modes for this, check Exif2.2 specs
            // If it just says 'unknown' or we don't know it, then
            // don't bother showing it - it doesn't add any useful information.
    }

    if (ImageInfo.MeteringMode > 0){ // 05-jan-2001 vcs
        printf("Metering Mode: ");
        switch(ImageInfo.MeteringMode) {
        case 1: printf("average\n"); break;
        case 2: printf("center weight\n"); break;
        case 3: printf("spot\n"); break;
        case 4: printf("multi spot\n");  break;
        case 5: printf("pattern\n"); break;
        case 6: printf("partial\n");  break;
        case 255: printf("other\n");  break;
        default: printf("unknown (%d)\n",ImageInfo.MeteringMode); break;
        }
    }

    if (ImageInfo.ExposureProgram){ // 05-jan-2001 vcs
        switch(ImageInfo.ExposureProgram) {
        case 1:
            printf("Exposure     : Manual\n");
            break;
        case 2:
            printf("Exposure     : program (auto)\n");
            break;
        case 3:
            printf("Exposure     : aperture priority (semi-auto)\n");
            break;
        case 4:
            printf("Exposure     : shutter priority (semi-auto)\n");
            break;
        case 5:
            printf("Exposure     : Creative Program (based towards depth of field)\n"); 
            break;
        case 6:
            printf("Exposure     : Action program (based towards fast shutter speed)\n");
            break;
        case 7:
            printf("Exposure     : Portrait Mode\n");
            break;
        case 8:
            printf("Exposure     : LandscapeMode \n");
            break;
        default:
            break;
        }
    }
    switch(ImageInfo.ExposureMode){
        case 0: // Automatic (not worth cluttering up output for)
            break;
        case 1: printf("Exposure Mode: Manual\n");
            break;
        case 2: printf("Exposure Mode: Auto bracketing\n");
            break;
    }

    if (ImageInfo.DistanceRange) {
        printf("Focus range  : ");
        switch(ImageInfo.DistanceRange) {
            case 1:
                printf("macro");
                break;
            case 2:
                printf("close");
                break;
            case 3:
                printf("distant");
                break;
        }
        printf("\n");
    }
}


//--------------------------------------------------------------------------
// Summarize highlights of image info on one line (suitable for grep-ing)
//--------------------------------------------------------------------------
void ShowConciseImageInfo(void)
{
    printf("\"%s\"",ImageInfo.FileName);

    printf(" %dx%d",ImageInfo.Width, ImageInfo.Height);

    if (ImageInfo.ExposureTime){
        if (ImageInfo.ExposureTime <= 0.5){
            printf(" (1/%d)",(int)(0.5 + 1/ImageInfo.ExposureTime));
        }else{
            printf(" (%1.1f)",ImageInfo.ExposureTime);
        }
    }

    if (ImageInfo.ApertureFNumber){
        printf(" f/%3.1f",(double)ImageInfo.ApertureFNumber);
    }

    if (ImageInfo.FocalLength35mmEquiv){
        printf(" f(35)=%dmm",ImageInfo.FocalLength35mmEquiv);
    }

    if (ImageInfo.FlashUsed >= 0 && ImageInfo.FlashUsed & 1){
        printf(" (flash)");
    }

    printf("\n");
}
