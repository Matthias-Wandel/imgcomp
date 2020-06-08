//-----------------------------------------------------------------------------------
// imgcomp image comparison main module
// Matthias Wandel 2015-2020
//
// Imgcomp is licensed under GPL v2 (see README.txt)
//-----------------------------------------------------------------------------------
#include <stdio.h>
#include <stdio.h>
#include <malloc.h>
#include <stddef.h>
#include <stdlib.h>
#include <memory.h>
#include "imgcomp.h"
#include "config.h"

int NewestAverageBright;

static ImgMap_t * DiffVal = NULL;
ImgMap_t * WeightMap = NULL;

static TriggerInfo_t SearchDiffMaxWindow(Region_t Region, int threshold);


//----------------------------------------------------------------------------------------
// Compare two images in memory
// Pic1 is previous pic, pic2 is latest pic.
//----------------------------------------------------------------------------------------
TriggerInfo_t ComparePix(MemImage_t * pic1, MemImage_t * pic2, char * DebugImgName)
{
    int width, height, bPerRow;
    int row, col;
    MemImage_t * DiffOut = NULL;
    int DiffHist[256];
    int a;
    int DetectionPixels;
    int m1i, m2i;
    double BrightnessRatio;
    Region_t MainReg;
    TriggerInfo_t RetVal;
    RetVal.x = RetVal.y = 0;
    RetVal.DiffLevel = -1;
    RetVal.Motion = 0;

    if (Verbosity){
        printf("\ncompare pictures %dx%d %d\n", pic1->width, pic1->height, pic1->components);
    }

    if (pic1->width != pic2->width || pic1->height != pic2->height 
        || pic1->components != pic2->components){
        fprintf(stderr, "pic types mismatch!\n");
        return RetVal;
    }
    width = pic1->width;
    height = pic1->height;
    bPerRow = width * 3;    

	// Make sure we have an allocated working array and weight map.
	
    if (DiffVal != NULL && (width != DiffVal->w || height != DiffVal->h)){
        // DiffVal allocated size is wrong.
        free(DiffVal);
        free(WeightMap);
        DiffVal = NULL;
    }
    if (DiffVal == NULL){
		DiffVal = MakeImgMap(width,height);
		
        if (!WeightMap){
            FillWeightMap(width,height);
        }else{
            if (WeightMap->w != width || WeightMap->h != height){
                fprintf(stderr,"diff map image size mismatch\n");
                exit(-1);
            }
        }
    }
    memset(DiffVal->values, 0,  sizeof(DiffVal->values)*width*height);

    if (DebugImgName){
		// Create image for writing difference to.
        int data_size;
        data_size = height * bPerRow;
        DiffOut = malloc(data_size+offsetof(MemImage_t, pixels));
        memcpy(DiffOut, pic1, offsetof(MemImage_t, pixels));
        memset(DiffOut->pixels, 0, data_size);
    }
	
    MainReg = Regions.DetectReg;
    if (MainReg.y2 > height) MainReg.y2 = height;
    if (MainReg.x2 > width) MainReg.x2 = width;
    if (MainReg.x2 < MainReg.x1 || MainReg.y2 < MainReg.y1){
        fprintf(stderr, "Negative region, or region outside of image\n");
        return RetVal;
    }

    DetectionPixels = (MainReg.x2-MainReg.x1) * (MainReg.y2-MainReg.y1);
    if (DetectionPixels < 1000){
        fprintf(stderr, "Too few pixels in region\n");
        return RetVal;
    }

    if (Verbosity > 0){
        printf("Detection region is %d-%d, %d-%d\n",MainReg.x1, MainReg.x2, MainReg.y1, MainReg.y2);
    }


    // Compute brightness multipliers for the two images
    {
        double b1average, b2average;
        double m1, m2;
        b1average = AverageBright(pic1, MainReg, WeightMap);
        b2average = AverageBright(pic2, MainReg, WeightMap);

        NewestAverageBright = (int)(b2average+0.5);

        if (Verbosity > 0){
            printf("average bright: %f %f\n",b1average, b2average);
        }
        if (b1average < 0.5) b1average = 0.5; // Avoid possible division by zero.
        if (b2average < 0.5) b2average = 0.5;

        m1 = 80/b1average;
        m2 = 80/b2average;

        {
            double maxm, minm;
            if (m1 > m2){
                maxm = m1; minm = m2;
            }else{
                maxm = m2; minm = m1;
            }
            BrightnessRatio = maxm/minm;

            if (maxm > 4.0){
                // Don't allow multiplier to get bigger than 4.  Otherwise, for dark images
                // we just end up multiplying pixel noise!
                m1 = m1 * 4 / maxm;
                m2 = m2 * 4 / maxm;
            }else if (maxm < 1.0){
                // And there's no point in scaling down both images either.
                m1 = m1 * 1 / maxm;
                m2 = m2 * 1 / maxm;
            }
        }

        if (Verbosity) printf("Brightness adjust multipliers:  m1 = %5.2f  m2=%5.2f\n",m1,m2);

        m1i = (int)(m1*256+0.5);
        m2i = (int)(m2*256+0.5);
    }

    // Compute differences
	memset(DiffHist, 0, sizeof(DiffHist));
    for (row=MainReg.y1;row<MainReg.y2;){
        unsigned char * p1, *p2, *pd = NULL;
        int * ExRow;
        int * diffrow;
        p1 = pic1->pixels+row*bPerRow + MainReg.x1*3;
        p2 = pic2->pixels+row*bPerRow + MainReg.x1*3;
        diffrow = &DiffVal->values[width*row];
        ExRow = &WeightMap->values[width*row];
        if (DebugImgName) pd = DiffOut->pixels+row*bPerRow;

        for (col=MainReg.x1;col<MainReg.x2;col++){
            if (ExRow[col]){
                // Data is in order red, green, blue.
                int dr,dg,db;
                int dcomp;
                int max;
                {
                    // Find maximum of red green or blue for either picture after adjustment.
                    int max1, max2;
                    max1 = p1[0];
                    if (p1[1] > max1) max1 = p1[1];
                    if (p1[2] > max1) max1 = p1[2];
                    max1 *= m1i;
                    max2 = p2[0];
                    if (p2[1] > max1) max1 = p2[1];
                    if (p2[2] > max1) max1 = p2[2];
                    max2 *= m2i;
                    max = max2 > max1 ? max2 : max1;
                    max = max >> 8;
                    if (max < 40) max = 40; // Don't allow under 40 to avoid amlifying dark noise.
                }

                // Compute red green and blue differences
                dr = (p1[0]*m1i - p2[0]*m2i);
                dg = (p1[1]*m1i - p2[1]*m2i);
                db = (p1[2]*m1i - p2[2]*m2i);
                
				// Make differences absolute values.
                if (dr < 0) dr = -dr;
                if (dg < 0) dg = -dg;
                if (db < 0) db = -db;
				
				// compute composite difference.
                dcomp = (dr + dg*2 + db) >> 8; // Put more emphasis on green
                dcomp = dcomp * 120/max;       // Normalize difference to local brightness.


				// Add computed difference value to histogram bins.
                if (dcomp >= 256) dcomp = 255;
                if (dcomp < 0){
                    fprintf(stderr,"Internal error dcomp\n");
                    dcomp = 0;
                }
                DiffHist[dcomp] += 1;
                diffrow[col] = dcomp;
        
                if (DebugImgName){
                    // Save the difference image, scale brightness up 4x
                    dr=(dr*60/max)>> 6; if (dr > 255) dr = 255;
                    dg=(dg*60/max)>> 6; if (dg > 255) dg = 255;
                    db=(db*60/max)>> 6; if (db > 255) db = 255;
                    pd[0] = dr;
                    pd[1] = dg;
                    pd[2] = db;
                }
            }

			// Advance pointers to next pixel.
            pd += 3;
            p1 += 3;
            p2 += 3;
        }
        row++;
    }


    if (DebugImgName){
        WritePpmFile(DebugImgName,DiffOut);
        free(DiffOut);
    }
   

    if (Verbosity > 1){
        printf("Difference histogram\n");
        for (a=0;a<60;a+=4){
            printf("%3d:%6d,%6d,%6d,%6d\n",a,DiffHist[a], DiffHist[a+1], DiffHist[a+2], DiffHist[a+3]);
        }
    }

    {
        // Try to gauge the noise level of the difference maps using the built histogram.
		// assuming two thirds of the image has not changed
        int cumsum = 0;
        int threshold;
        int twothirds = DetectionPixels*2/3;
        TriggerInfo_t Trigger;
        
        for (a=0;a<256;a++){
            if (cumsum >= twothirds) break;
            cumsum += DiffHist[a];
        }

        threshold = a*3+12;
        if (threshold < 30) threshold = 30;
		
        int maxth = 180 + BrightnessRatio*5;
        if (threshold > maxth){
			threshold = maxth;
			if (Verbosity) printf("Using limit threshold of %d\n", threshold);			
		}else{
			if (Verbosity) printf("2/3 of image is below %d diff.  Using %d threshold\n",a, threshold);
		}

		// Search for a window with the largest difference in it
        Trigger = SearchDiffMaxWindow(MainReg, threshold);
        return Trigger;
    }
}

//----------------------------------------------------------------------------------------
// Search for an N x N window with the maximum differences in it.
// This for rejecting spurious differences outdoors where grass and leaves can cause a
// lot of weak spurious motion over large areas.
//----------------------------------------------------------------------------------------
static TriggerInfo_t SearchDiffMaxWindow(Region_t Region, int threshold)
{
    TriggerInfo_t retval;
            
    // Scale down by this factor before applying windowing algorithm to look for max localized change
    // Thee factors work well with source images that are 1000-2000 pixels across.
    const int scalef = 5;

    // these determine the window over which to look for the change (after scaling)    
    const int wind_w = 4, wind_h = 7;
    
	static int widthSc, heightSc;
	static ImgMap_t * DiffScaled = NULL;
	static ImgMap_t * DiffScaledBf = NULL;
    static ImgMap_t * Fatigue = NULL;
	static ImgMap_t * FatigueBl = NULL;
	
	// Allocate working arrays if necessary.
    if (DiffScaled == NULL){
        #define ROOF(x) ((x+scalef-1)/scalef)
        widthSc = ROOF(DiffVal->w);
        heightSc = ROOF(DiffVal->h);
		DiffScaled = MakeImgMap(widthSc, heightSc);
		DiffScaledBf = MakeImgMap(widthSc, heightSc);
        Fatigue = MakeImgMap(widthSc, heightSc);
		FatigueBl = MakeImgMap(widthSc, heightSc);
    }else{
        if (widthSc != ROOF(DiffVal->w) || heightSc != ROOF(DiffVal->h)){
            fprintf(stderr, "image size changed, error!");
            // Could reallocate, but probably other code doesn't handle resizing anyway.
            exit(-1);
        }
    }
    
    // Compute scaled down array of differences.  Destination: DiffScaled[]
    int width = DiffVal->w;
    memset(DiffScaled->values, 0, sizeof(int)*widthSc*heightSc);
    for (int row=Region.y1;row<Region.y2;row++){
        // Compute difference by column using established threshold value
        int * diffrow, *ExRow;
        int * widthScrow;
        diffrow = &DiffVal->values[width*row];
        ExRow = &WeightMap->values[width*row];

        widthScrow = &DiffScaled->values[widthSc*(row/scalef)];
        for (int col=Region.x1;col<Region.x2;col++){
            int d = diffrow[col] - threshold;
            if (d > 0){
                widthScrow[col/scalef] += d;
                if (ExRow[col] > 1){
                    
                    // Double weight region
                    widthScrow[col/scalef] += d;
                }
            }
        }
    }
    

    if (Verbosity > 1){
        printf("Scaled difference array (%d x %d)\n", widthSc, heightSc);
		ShowImgMap(DiffScaled, 100);
    }

    if (MotionFatigueTc > 0){
        static int SinceFatiguePrint = 0;
		int FatigueAverage;
        // Compute motion fatigue
        FatigueAverage = 0;
        for (int row=0;row<heightSc;row++){
            for (int col=0;col<widthSc;col++){
                int ds, nFat;
                ds = DiffScaled->values[row*widthSc+col];
                nFat = (Fatigue->values[row*widthSc+col]*(MotionFatigueTc-1) + ds)/MotionFatigueTc; // Expontential decay on it.
                Fatigue->values[row*widthSc+col] = nFat;
                FatigueAverage += nFat;
            }
        }
        FatigueAverage = FatigueAverage/(heightSc*widthSc); // Divide by array size to get average.
        
        // Print fatigue map to log from time to time.
        if (Verbosity > 1 || (FatigueAverage > 50 && SinceFatiguePrint > 60)){
            // Print the fatigure array every minuts if there is stuff in it.
            fprintf(Log, "Fatigue map (%d x %d) sum=%d<small>\n", widthSc, heightSc, FatigueAverage);
			ShowImgMap(Fatigue, 50);
            fprintf(Log, "</small>\n");
            SinceFatiguePrint = 0;
        }
        SinceFatiguePrint++;
    
		// Subtract out motion fatigue
		BloomImgMap(Fatigue, FatigueBl); // Use max of cell and eight neighbours for motion fatigue.
		//BloomImgMap(FatigueBl, FatigueBl2); // Use max of cell and eight neighbours for motion fatigue.
		
        for (int row=0;row<heightSc;row++){
            for (int col=0;col<widthSc;col++){
                int ds, FatM;
                FatM = FatigueBl->values[row*widthSc+col];
                ds = DiffScaled->values[row*widthSc+col] - FatM*3;
                if (ds < 0) ds = 0;
                DiffScaled->values[row*widthSc+col] = ds;
            }
        }
    }

	int maxc, maxr, maxval;
	
	// Now search for the maximum inside a rectangular window.
	maxval = BlockFilterImgMap(DiffScaled, DiffScaledBf, wind_w, wind_h, &maxc, &maxr);
	retval.DiffLevel = maxval/100;
    retval.x = retval.x * scalef - maxc * scalef / 2;
    retval.y = retval.y * scalef - maxr * scalef / 2;
	
	// Work out where most of the motion was found inside the search window.
    if (Verbosity) printf("Window contents.  Cols %d-%d Rows %d-%d\n",maxc,maxc+wind_w-1,maxr, maxr+wind_h-1);
    if (maxval > 0){
        double xsum, ysum;
        int sum;
		int row = maxr;
		if (row < 0) row = 0;
		
        xsum = ysum = sum = 0;
        for (;row<maxr+wind_h;row++){
            int col = maxc; 
            if (col < 0) col = 0;
            for(;col<maxc+wind_w;col++){
                int v = DiffScaled->values[row*widthSc+col];
                xsum += col*v;
                ysum += row*v;
                sum += v;
                //printf("%5d",DiffScaled->values[row*widthSc+col]);
            }
            //printf("\n");
        }
        if (Verbosity) printf("Exact col=%5.1f, row=%5.1f\n",xsum*1.0/sum, ysum*1.0/sum);
        retval.x = (int)(xsum*scalef*ScaleDenom/sum)+scalef*ScaleDenom/2;
        retval.y = (int)(ysum*scalef*ScaleDenom/sum)+scalef*ScaleDenom/2;
        if (Verbosity) printf("Picture coordinates: x,y = %d,%d\n",retval.x, retval.y);
    }
   
    return retval;
}

  