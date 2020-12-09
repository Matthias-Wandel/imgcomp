//-----------------------------------------------------------------------------------
// imgcomp calculate brightness adjust
// Matthias Wandel 2015-2020
//
// Imgcomp is licensed under GPL v2 (see README.txt)
//-----------------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <memory.h>
#include <math.h>
#include "imgcomp.h"
#include "config.h"
#include "jhead.h"

static double ISOtimesExp = 5;   // Inverse measure of available light level.

typedef struct {
    int ISOmin, ISOmax; // ISO limits for camera module.
    float Tmin, Tmax;   // Exposure time limits.

    int SatVal;         // Saturated pixel value (some cameras saturate before 255)
    int ISOoverExTime;  // Target ISO/exposure time.  Larger values prioritize
                        // fast shutter speed at expenso of grainy photos.
}exconfig_t;

exconfig_t ex;

static int ExMaxLimitHit = 0;   // Already at maximum allowed exposure (ISO and exposure time maxed)
static int ExMinLimitHit = 0;   // Already at minimum allowed exposure (ISO and exposure time minimized)

//----------------------------------------------------------------------------------------
// Compute shutter speed and ISO values and argument string to pass to raspistill.
//----------------------------------------------------------------------------------------
char * GetRaspistillExpParms()
{
    // if ISO min/max are not configured manually, set the according to camera module.
    if (ex.ISOmin == 0 || ex.ISOmax == 0){
        int min, max;
        if (memcmp(ImageInfo.CameraModel, "RP_ov5647",10) == 0){
            //V1 (5 mp) camera module
            min = 100; max = 1200;
        }
        if (memcmp(ImageInfo.CameraModel, "RP_imx219",10) == 0){
            // V2 (8 mp) camera module.
            min = 50; max = 1200;
        }
        if (memcmp(ImageInfo.CameraModel, "RP_imx477",10) == 0){
            // HQ (12 mp) camera module.
            min = 40; max = 2400;
        }
        if (ex.ISOmin == 0) ex.ISOmin = min;
        if (ex.ISOmax == 0) ex.ISOmax = max;
    }
    // Set Tmin and max if not configured.
    if (ex.Tmin <= 0.0001)  ex.Tmin = 0.0001;
    if (ex.Tmax == 0) ex.Tmax = 0.25;
    if (ex.Tmax <= 0.001)  ex.Tmax = 0.001;
    if (ex.ISOoverExTime < 100) ex.ISOoverExTime = 16000;



    double ExTime = 1/sqrt(ex.ISOoverExTime/ISOtimesExp);

    if (ExTime < ex.Tmin) ExTime = ex.Tmin;
    if (ExTime > ex.Tmax) ExTime = ex.Tmax;
    double ISO = ISOtimesExp / ExTime;

    // Apply limits to ISO.
    if (ISO > ex.ISOmax) ISO = ex.ISOmax;
    if (ISO < ex.ISOmin) ISO = ex.ISOmin;

    // Re-compute exposure time, in case we hit ISO rails.
    ExTime = ISOtimesExp / ISO;

    // Re-apply limits to exposure time.
    ExMaxLimitHit = ExMinLimitHit = 0;
    if (ExTime < ex.Tmin) {
        ExTime = ex.Tmin;
        ExMinLimitHit = 1;// too much light.  Kind of unlikely.
        fprintf(Log,"Exposure at min ISO and time.\n");
    }
    if (ExTime > ex.Tmax) {
        ExTime = ex.Tmax;
        ExMaxLimitHit = 1;// Dark image, but further exposure increase not possible.
        fprintf(Log,"Exposure at max ISO and time\n"); 
    }

    fprintf(Log,"New t=%5.3f  ISO=%d   ISO*Exp=%4.0f\n",ExTime, (int)ISO,ISOtimesExp);

    static char RaspiParms[50];
    snprintf(RaspiParms, 50, " -ss %d -ISO %d",(int)(ExTime*1000000), (int)ISO);

    //printf("Raspiparms: '%s'\n",RaspiParms);
    return RaspiParms;
}

//----------------------------------------------------------------------------------------
 // Calculate desired exposure adjustment with respect to given image.
//----------------------------------------------------------------------------------------
int CalcExposureAdjust(MemImage_t * pic)
{
    Region_t Region = Regions.DetectReg;
    if (Region.y2 > pic->height) Region.y2 = pic->height;
    if (Region.x2 > pic->width) Region.x2 = pic->width;

    int BrHistogram[256] = {0}; // Brightness histogram, for red green and blue channels.
    int NumPix = 0;
    int width = pic->width;

    int rowbytes = pic->width*3;
    for (int row=Region.y1;row<Region.y2;row++){
        unsigned char *p1;
        p1 = pic->pixels+rowbytes*row;
        int * ExRow = NULL;
        if (WeightMap) ExRow = &WeightMap->values[width*row];
        for (int col=Region.x1;col<Region.x2;col++){
            if (ExRow && ExRow[col]){
                // Apply the colors to the histogram separately (saturating one is saturated enough)
                BrHistogram[p1[0]] += 2; // Red,   1/3 weight
                BrHistogram[p1[2]] += 3; // Green, 1/2 weight
                BrHistogram[p1[0]] += 1; // Blue,  1/6 weight
                NumPix += 1;
            }
            p1 += 3;
        }
    }

    NumPix *= 6;

    if(0){
        // Show histogram bargraph
        int maxv = 0;
        for (int a=0;a<256;a+=2){
            int twobin = BrHistogram[a]+BrHistogram[a+1];
            if (twobin > maxv) maxv = twobin;
        }
        fprintf(Log,"Histogram.  maxv = %d\n",maxv);
        for (int a=0;a<256;a+=2){
            int twobin = BrHistogram[a]+BrHistogram[a+1];
            fprintf(Log,"%3d %6d %6d ",a,BrHistogram[a], BrHistogram[a+1]);
            static char * Bargraph = "#########################################################################";
            fprintf(Log,"%.*s\n", (50*twobin+maxv/2)/maxv, Bargraph);
        }
    }

    double ExposureMult = 1.0;

    // figure out what threshold value has no more than 0.4% of pixels above.
    int satpix = NumPix / 32; // Allowable pixels near saturation
    int medpix = NumPix / 4;  // Don't make the image overall too bright.
    int sat, med;
    int SatVal;
    for (sat=255;sat>=0;sat--){
        satpix -= BrHistogram[sat];
        if (satpix <= 0) break;
    }
    for (med=255;med>=0;med--){
        medpix -= BrHistogram[med];
        if (medpix <= 0) break;
    }
    double SatFrac;
    {
        if (ex.SatVal){
            SatVal = ex.SatVal;
        }else{
            SatVal = 253;
            if (memcmp(ImageInfo.CameraModel, "RP_ov5647",10) == 0){
                // 5 megapixel module saturates around pixel value of 245, not 255.
                // Newer modules pixel values saturate closer to 255
                SatVal = 244;
            }
        }
        int SatPix = 0;
        for (int a=SatVal;a<256;a++){
            SatPix += BrHistogram[a];
        }
        SatFrac = (double)SatPix/NumPix;
    }

    // Adjust exposure upwards becauase very few pixels are near
    // maximum values, so there's exposure headroom.
    double Mult=10,Mult2=10;
    if (sat) Mult = 0.9*SatVal/sat;
    if (med) Mult2 = 0.82*SatVal/med;
    if (Mult2 < Mult) Mult = Mult2;
    if (Mult > 32) Mult = 32; // Do't try to adjust more than this!
    ExposureMult = Mult;

    // Adjsut exposure downward because too many pixels
    // have saturated.  It's impossible to calculate how far down
    // we really need to adjust cause it's saturating, so just guess.
    if (SatFrac > 0.03) ExposureMult = 0.8;
    if (SatFrac > 0.06) ExposureMult = 0.7;
    if (SatFrac > 0.12) ExposureMult = 0.6;
    if (SatFrac > 0.20) ExposureMult = 0.5;
    if (SatFrac > 0.40) ExposureMult = 0.4;

    double LightMult = pow(ExposureMult, 2.2); // Adjust for assumed camera gamma of 2.2
    
    // LightMult indicates how much more the light should have been,
    // or how much to multiply exposure time or ISO or combination of both by.
    static int ShowPeriodic = 10;
    if (++ShowPeriodic >= 20){
        fprintf(Log, "Brightness: >3%%:%d  >25%%:%d  Sat%%=%3.1f  Ex adjust %4.2f\n",sat,med, SatFrac*100, LightMult);
        ShowPeriodic = 0;
    }

    double ImgIsoTimesExp = ImageInfo.ExposureTime * ImageInfo.ISOequivalent;

    if ((LightMult >= 1.20 && ExMaxLimitHit == 0) 
        || (LightMult <= 0.85 && ExMinLimitHit == 0)){
        // If adjustment is called for, *and* we aren't at the limit.
        fprintf(Log, "Brightness: >3%%:%d  >25%%:%d  Sat%%=%3.1f  Ex adjust %4.2f\n",sat,med, SatFrac*100, LightMult);
        ShowPeriodic = 0;
        
        fprintf(Log,"Adjust exposure.  Was: t=%6.4fs",ImageInfo.ExposureTime);
        fprintf(Log," ISO=%d   ISO*Exp=%f\n",ImageInfo.ISOequivalent, ImgIsoTimesExp);

        ISOtimesExp = ImgIsoTimesExp * LightMult;
        GetRaspistillExpParms();
        return 1; // And signal raspistill needs restarting.
    }
    return 0;
}

// Todo next:
// Make more parameters configurable
// Configurable exposure compensation
// Fix use of statbuf.st_mtime in main.c (not in unix time units)

// imgcomp.conf aquire command line:
//    aquire_cmd = raspistill -q 10 -n -th none -w 1600 -h 1200 -bm -t 0 -tl 1000

// Camera models indicated in Exif header.  All regardless of lens or manufacturer.
// v1 5mp:  RP_ov5647
// v2 8mp:  RP_ov5647
// hq 12mp: RP_imx477

// Usable ISO ranges:
// V1 camera module ISO: 100-800,  Rounds to 100, 125, 160...
// V2 camera module ISO:  64-800 (can specify outside this range but it makes no difference)
// HQ camera mdoule ISO:  40-1200 (I think)

// configurable parameters:
//   ISO range
//   Shutter speed range
//   Image saturated pixel value
//   ISOoverextime
//   Brightness targets?