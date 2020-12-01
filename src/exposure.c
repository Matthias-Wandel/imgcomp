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

static double ISOoverExTime = 4000; // Configured ISO & exposure time relationship.

static int ISOmin = 0, ISOmax = 0;
//----------------------------------------------------------------------------------------
// Compute shutter speed and ISO values and argument string to pass to raspistill.
//----------------------------------------------------------------------------------------
char * GetRaspistillExpParms()
{
    // if ISO min/max are not configured manually, set the according to camera module.
    if (ISOmin == 0 || ISOmax == 0){
        int min, max;
        if (memcmp(ImageInfo.CameraModel, "RP_ov5647",10) == 0){
            //V1 (5 mp) camera module
            min = 100; max = 800;
        }
        if (memcmp(ImageInfo.CameraModel, "RP_imx219",10) == 0){
            // V2 (8 mp) camera module.
            min = 50; max = 800;
        }
        if (memcmp(ImageInfo.CameraModel, "RP_imx477",10) == 0){
            // HQ (12 mp) camera module.
            min = 40; max = 1250;
        }
        if (ISOmin == 0) ISOmin = min;
        if (ISOmax == 0) ISOmax = max;
    }

    double ExTime = 1/sqrt(ISOoverExTime/ISOtimesExp);

    if (ExTime < 0.001) ExTime = 0.001;
    if (ExTime > 0.5) ExTime = 0.5;
    double ISO = ISOtimesExp / ExTime;

    // Apply limits to ISO.
    if (ISO > ISOmax) ISO = ISOmax;
    if (ISO < ISOmin) ISO = ISOmin;

    // Re-compute exposure time, in case we hit ISO rails.
    ExTime = ISOtimesExp / ISO;

    // Re-apply limits to exposure time.
    if (ExTime < 0.001) ExTime = 0.001;
    if (ExTime > 0.5) ExTime = 0.5;



    printf("New t=%5.3f  ISO=%d",ExTime, (int)ISO);
    printf("   ISO*Exp=%4.0f\n",ISOtimesExp);

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


    int rowbytes = pic->width*3;
    for (int row=Region.y1;row<Region.y2;row++){
        unsigned char *p1;
        p1 = pic->pixels+rowbytes*row;
        //px = &WeightMap->values[pic->width*row];
        for (int col=Region.x1;col<Region.x2;col++){
            //if (px[col]){
                // Apply the colors to the histogram separately (saturating one is saturated enough)
                BrHistogram[p1[0]] += 2; // Red,   1/3 weight
                BrHistogram[p1[2]] += 3; // Green, 1/2 weight
                BrHistogram[p1[0]] += 1; // Blue,  1/6 weight
                NumPix += 1;
            //}
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
        printf("Histogram.  maxv = %d\n",maxv);
        for (int a=0;a<256;a+=2){
            int twobin = BrHistogram[a]+BrHistogram[a+1];
            printf("%3d %6d %6d ",a,BrHistogram[a], BrHistogram[a+1]);
            static char * Bargraph = "#########################################################################";
            printf("%.*s\n", (50*twobin+maxv/2)/maxv, Bargraph);
        }
    }

    double ExposureMult = 1.0;

    // figure out what threshold value has no more than 0.4% of pixels above.
    int satpix = NumPix / 32; // Allowable pixels near saturation
    int medpix = NumPix / 4;  // Don't make the image overall too bright.
    int sat, med;
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
        int SatPix = 0;
        int sat = 253;
        if (memcmp(ImageInfo.CameraModel, "RP_ov5647",10) == 0){
            // 5 megapixel module saturates around pixel value of 245, not 255.
            // Newer modules pixel values saturate closer to 255
            sat = 244;
        }
        for (;sat<256;sat++){
            SatPix += BrHistogram[sat];
        }
        SatFrac = (double)SatPix/NumPix;
    }

    printf("Brightness: >3%%:%d  >25%%:%d  Sat%%=%3.1f\n",sat,med, SatFrac*100);
    if (sat < 220){
        // Adjust exposure upwards becauase very few pixels are near
        // maximum values, so there's exposure headroom.
        double Mult=10,Mult2=10;
        if (sat) Mult = 230.0/sat;
        if (med) Mult2 = 210.0/med;
        if (Mult2 < Mult) Mult = Mult2;
        if (Mult > 32) Mult = 32; // Do't try to adjust more than this!

        ExposureMult = Mult;
    }else{
        // Adjsut exposure downward because too many pixels
        // have saturated.  It's impossible to calculate how far down
        // we really need to adjust cause it's saturating, so just guess.
        if (SatFrac > 0.03) ExposureMult = 0.8;
        if (SatFrac > 0.06) ExposureMult = 0.7;
        if (SatFrac > 0.12) ExposureMult = 0.6;
        if (SatFrac > 0.20) ExposureMult = 0.5;
        if (SatFrac > 0.40) ExposureMult = 0.4;
    }

    double LightMult = pow(ExposureMult, 2.2); // Adjust for assumed camera gamma of 2.2
    
    // LightMult indicates how much more the light should have been,
    // or how much to multiply exposure time or ISO or combination of both by.
    printf("Pix mult: %4.2f  f-stop adjust: %5.2f\n",ExposureMult, log(LightMult)/log(2));

    double ImgIsoTimesExp = ImageInfo.ExposureTime * ImageInfo.ISOequivalent;

    ISOtimesExp = ImgIsoTimesExp;

    if (LightMult >= 1.25 || LightMult <= 0.8){
        printf("Adjust exposure.  Was: t=%6.4fs",ImageInfo.ExposureTime);
        printf(" ISO=%d   ISO*Exp=%f\n",ImageInfo.ISOequivalent, ImgIsoTimesExp);

        ISOtimesExp *= LightMult;
        GetRaspistillExpParms();
        return 1; // And cause raspistill restart.
        // And cause raspistill restart.
    }
    return 0;
}



// Todo next:
// Use weight map for exposure calculation
// Limits to ISO range and shutter speed?
// Make more parameters configurable
// Make exposure stuff print less (maybe print brightness once a minute unless it changed)

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