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

static double ISOtimesExp = 0.2;  // Inverse measure of available light level, initial value.

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
            min = 100; max = 800;
        }
        if (memcmp(ImageInfo.CameraModel, "RP_imx219",10) == 0){
            // V2 (8 mp) camera module.
            min = 50; max = 1600;
        }
        if (memcmp(ImageInfo.CameraModel, "RP_imx477",10) == 0){
            // HQ (12 mp) camera module.
            min = 40; max = 3200;
        }
        if (ex.ISOmin == 0) ex.ISOmin = min;
        if (ex.ISOmax == 0) ex.ISOmax = max;
    }
    // Set Tmin and max if not configured.
    if (ex.Tmin <= 0.0001)  ex.Tmin = 0.0001;
    if (ex.Tmax == 0) ex.Tmax = 0.25;
    if (ex.Tmax <= 0.001)  ex.Tmax = 0.001;
    if (ex.ISOoverExTime == 0) ex.ISOoverExTime = 32000;



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

    fprintf(Log,"New exposure: t=%0.5f ISO=%d ISO*Exp=%5.0f\n",ExTime, (int)ISO,ISOtimesExp);

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
                BrHistogram[p1[1]] += 3; // Green, 1/2 weight
                BrHistogram[p1[2]] += 1; // Blue,  1/6 weight
                NumPix += 1;
            }
            p1 += 3;
        }
    }
    NumPix *= 6;

    static int ShowPeriodic = 45;
    if (++ShowPeriodic >= 60) ShowPeriodic = 0;

    if (ShowPeriodic == 0){
        // Show histogram bargraph
        int maxv = 0;
        int skz = 1;
        for (int a=0;a<256;a+=2){
            int twobin = BrHistogram[a]+BrHistogram[a+1];
            if (twobin > maxv) maxv = twobin;
        }
        fprintf(Log,"Brighness histogram\n");
        for (int a=0;a<256;a+=2){
            if (a == 26){
                fprintf(Log,"...........\n");// Skip middle part of histogram, not that interesting.
                a = 210;
                skz = 1;
            }
            int twobin = BrHistogram[a]+BrHistogram[a+1];
            if (twobin == 0 && skz) continue;
            skz = 0;
            fprintf(Log,"%3d %6d %6d ",a,BrHistogram[a], BrHistogram[a+1]);
            static char * Bargraph = "#########################################################################";
            fprintf(Log,"%.*s\n", (50*twobin+maxv/2)/maxv, Bargraph);
        }
    }

    double ExposureMult = 1.0;

    // figure out what threshold value has no more than 0.4% of pixels above.
    int satpix = NumPix / 64;  // Allowable pixels near saturation
    int medpix = NumPix / 20;  // Don't make the image overall too bright.
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
    if (ex.SatVal){
        SatVal = ex.SatVal;
    }else{
        SatVal = 253;
        if (memcmp(ImageInfo.CameraModel, "RP_ov5647",10) == 0){
            // 5 megapixel module saturates around pixel value of 245, not 255.
            // Newer modules pixel values saturate closer to 255
            SatVal = 240;
        }
    }
    int SatPix = 0;
    for (int a=SatVal;a<256;a++){
        SatPix += BrHistogram[a];
    }
    SatFrac = (double)SatPix/NumPix;


    // Adjust exposure upwards becauase very few pixels are near
    // maximum values, so there's exposure headroom.
    double Mult=10,Mult2=10;
    if (sat) Mult = 0.96*SatVal/sat;
    if (med) Mult2 = 0.93*SatVal/med;
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

    double LightMult = pow(ExposureMult, 2.2); // assume camer gamma is 2.2 for light level calculation

    // LightMult indicates how much more the light should have been,
    // or how much to multiply exposure time or ISO or combination of both by.

    double ImgIsoTimesExp = ImageInfo.ExposureTime * ImageInfo.ISOequivalent;
    
    if (ShowPeriodic == 0){
        fprintf(Log, "Brightness: 3%%>%d  25%%>%d  Sat%%=%3.1f  Iso*Ex=%4.1f Adjust *%4.2f\n",sat,med, SatFrac*100, ImgIsoTimesExp, LightMult);
        ShowPeriodic = 0;
    }

    // Compute a measure of how steady the light is.  If it's really steady,
    // ten lower the threshold for making exposure adjustments.
    static double LightMult_ra = 1;
    static double LightMultVariance_ra = 1;
    const double newweight = 0.05;
    LightMult_ra = LightMult_ra*(1-newweight)+LightMult*newweight;
    double diff  = LightMult-LightMult_ra;
    LightMultVariance_ra = LightMultVariance_ra*(1-newweight) + diff*diff*newweight;
    if (ShowPeriodic == 0) fprintf(Log, "Mult:%5.2f mra:%5.2f stdev:%5.2f\n",LightMult, LightMult_ra, sqrt(LightMultVariance_ra));

    double ExposureThresholdRatio = 1.2;
    if (LightMultVariance_ra < (0.2*0.2)){
        // Light has been very steady for a while, so be willing to make fine adjustments to exposure.
        ExposureThresholdRatio = 1.06;
        //printf("fine ");
    }

    if ((LightMult >= ExposureThresholdRatio && ExMaxLimitHit == 0)
        || (LightMult <= 1/ExposureThresholdRatio && ExMinLimitHit == 0)){
        // If adjustment is called for, *and* we haven't hit an exposure limit:
        fprintf(Log, "Brightness: 1.5%%>%d  5%%>%d  Sat%%=%3.1f Adjust *%4.2f\n",sat,med, SatFrac*100, LightMult);
        ShowPeriodic = 0;

        fprintf(Log,"Adjust exposure.  Was: t=%0.5f",ImageInfo.ExposureTime);
        fprintf(Log," ISO=%d   ISO*Exp=%.0f\n",ImageInfo.ISOequivalent, ImgIsoTimesExp);

        ISOtimesExp = ImgIsoTimesExp * LightMult;
        GetRaspistillExpParms();

        // Reset exposure adjust running averages (avoid fine adjust too soon)
        LightMult_ra = 1;
        LightMultVariance_ra = 1;

        return 1; // And signal raspistill needs restarting.
    }
    return 0;
}


