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




static double ISOtimesExp = 25.6;   // Inverse measure of available light level.

static double ISOoverExTime = 4000; // Configured ISO & exposure time relationship.
                                    // Change this value to change iso/

//----------------------------------------------------------------------------------------
// Compute shutter speed and ISO values to pass to raspistill.
//----------------------------------------------------------------------------------------
static void ComputeExpValues()
{
    double ExTime = 1/sqrt(ISOoverExTime/ISOtimesExp);
    
    // Apply shutter speed limits.
    if (ExTime < 0.0001) ExTime = 0.0001;
    if (ExTime > 0.5) ExTime = 0.5;
    
    double ISO = ISOtimesExp / ExTime;
    if (ISO > 1000) ISO = 1000;
    if (ISO < 25) ISO = 25; 
    
    printf("ISO/t = %6.0f",ISOoverExTime);
    printf("  t=%6.3f  ISO=%d\n",ExTime, (int)ISO);
    
    printf("Argument: -ss %d  -ISO %d\n",(int)(ExTime*1000000), (int)ISO);
}

//----------------------------------------------------------------------------------------
 // Calculate desired exposure adjustment with respect to given image.
//----------------------------------------------------------------------------------------
double CalcExposureAdjust(MemImage_t * pic)
{
    printf("Calc brightness\n");
    
    Region_t Region = Regions.DetectReg;
    if (Region.y2 > pic->height) Region.y2 = pic->height;
    if (Region.x2 > pic->width) Region.x2 = pic->width;


    int BrHistogram[256] = {0}; // Brightness histogram, for red green and blue channels.
    //memset(BrHistogram, 0, sizeof(BrHistogram));
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
    
    if (sat < 220){
        double Mult=10,Mult2=10;
        if (sat) Mult = 250.0/sat;
        if (med) Mult2 = 240.0/med;
        if (Mult2 < Mult) Mult = Mult2;
        if (Mult > 32) Mult = 32; // Max adjustment.
        
        ExposureMult = Mult;
    }else{
        // Consider last 4 values saturaged (because of jpeg compression noise)
        int SatPix = BrHistogram[255] + BrHistogram[254]
                   + BrHistogram[253] + BrHistogram[252];
                   
        double SatFrac = (double)SatPix/NumPix;
        //printf("Saturated fraction: %f\n",SatFrac);
        if (SatFrac > 0.04) ExposureMult = 0.8;
        if (SatFrac > 0.08) ExposureMult = 0.7;
        if (SatFrac > 0.16) ExposureMult = 0.6;
        if (SatFrac > 0.25) ExposureMult = 0.5;
        if (SatFrac > 0.40) ExposureMult = 0.4;
        if (SatFrac > 0.50) ExposureMult = 0.3;
        
    }
    
    printf("Pixel value multiplier: %f\n",ExposureMult);
    
    double LightMult = pow(ExposureMult, 2.2); // Adjust for assumed gamma of 2.2
    // LightMult indicates how much more the light should have been,
    // or how much to multiply exposure time or ISO or combination of both by.
    
    printf("f-stop adjustment: %5.2f\n",log(LightMult)/log(2));
    
    ISOtimesExp *= LightMult;
    
    ComputeExpValues();
    return LightMult;
}

