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

//----------------------------------------------------------------------------------------
// Compute shutter speed and ISO values and argument string to pass to raspistill.
//----------------------------------------------------------------------------------------
char * GetRaspistillExpParms()
{
    double ExTime = 1/sqrt(ISOoverExTime/ISOtimesExp);
    
    // Apply shutter speed limits.
    if (ExTime < 0.0001) ExTime = 0.0001;
    if (ExTime > 0.5) ExTime = 0.5;

    double ISO = ISOtimesExp / ExTime;
    if (ISO > 1000) ISO = 1000;
    if (ISO < 25) ISO = 25; 
    

    printf("New t=%6.3f  ISO=%d",ExTime, (int)ISO);
    printf("   ISO*Exp=%6.0f\n",ISOtimesExp);
    
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
    printf("Calc brightness\n");
    
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
            //if (a > 10 && a < 220) a += 2;
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
    
    printf("sat = %d  med=%d\n",sat,med);
    if (sat < 220){
        double Mult=10,Mult2=10;
        if (sat) Mult = 230.0/sat;
        if (med) Mult2 = 210.0/med;
        if (Mult2 < Mult) Mult = Mult2;
        if (Mult > 32) Mult = 32; // Max adjustment.
        
        ExposureMult = Mult;
    }else{
        // Depending on pi camera module, saturation level is different.
        // 5 megapixel module saturates around 245, not 255.
        // Newer modules saturate at 255, like they should.
        int SatPix = 0;
        int sat = 253;
        if (memcmp(ImageInfo.CameraModel, "RP_ov5647",10) == 0){
            // Depending on pi camera module, saturation level is different.
            // 5 megapixel module saturates around 245, not 255.
            // Newer modules pixel values saturate closer to 255
            sat = 244;
        }
        for (;sat<256;sat++){
            SatPix += BrHistogram[sat];
        }

        double SatFrac = (double)SatPix/NumPix;
        printf("Saturated fraction: %f\n",SatFrac);
        if (SatFrac > 0.03) ExposureMult = 0.8;
        if (SatFrac > 0.06) ExposureMult = 0.7;
        if (SatFrac > 0.12) ExposureMult = 0.6;
        if (SatFrac > 0.20) ExposureMult = 0.5;
        if (SatFrac > 0.40) ExposureMult = 0.4;
        
    }
    
    printf("Pixel value multiplier: %f\n",ExposureMult);
    
    double LightMult = pow(ExposureMult, 2.2); // Adjust for assumed gamma of 2.2
    // LightMult indicates how much more the light should have been,
    // or how much to multiply exposure time or ISO or combination of both by.
    
    printf("f-stop adjustment: %5.2f\n",log(LightMult)/log(2));


    double ImgIsoTimesExp = ImageInfo.ExposureTime * ImageInfo.ISOequivalent;
    printf("From jpeg: t=%6.4fs",ImageInfo.ExposureTime);
    printf(" ISO=%d   ISO*Exp=%f\n",ImageInfo.ISOequivalent, ImgIsoTimesExp);
    
    ISOtimesExp = ImgIsoTimesExp;

    
    if (LightMult >= 1.25 || LightMult <= 0.8){
        ISOtimesExp *= LightMult;
        GetRaspistillExpParms();
        return 1; // And cause raspistill restart.
        // And cause raspistill restart.
    }
    return 0;
}



// Todo next:
// Make all this only if option is turned on.
// If exposure management enabled, check that aquire command doesn't contain -o option
// Get rid of old brmonitor option
// Detection of last jpg in do directory breaks if other files present in /ramdisk.

// imgcomp.conf aquire command line:
//    aquire_cmd = raspistill -q 10 -n -th none -w 1600 -h 1200 -bm -t 0 -tl 1000


// Camera models:
// Workshop fisheye: RP_ov5647
// Frontdoor: RP_ov5647
// Driveway: RP_imx219
// Backyard: RP_imx219
// Driveway tele, garage_wb RP_imx477