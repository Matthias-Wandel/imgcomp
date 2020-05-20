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

typedef struct {
    int w, h;
    unsigned char values[1];
}ImgMap_t;

static ImgMap_t * DiffVal = NULL;
static ImgMap_t * WeightMap = NULL;

static TriggerInfo_t SearchDiffMaxWindow(Region_t Region, int threshold);

//----------------------------------------------------------------------------------------
// Calculate average brightness of an image.
//----------------------------------------------------------------------------------------
static double AverageBright(MemImage_t * pic, Region_t Region, ImgMap_t* WeightMap)
{
    double baverage;//
    int DetectionPixels;
    int row;
    int rowbytes = pic->width*3;
    DetectionPixels = 0;
 
    // Compute average brightnesses.
    baverage = 0;//rzaverage = 0;
    for (row=Region.y1;row<Region.y2;row++){
        unsigned char *p1, *px;
        int col, brow = 0;//, redrow = 0;
        p1 = pic->pixels+rowbytes*row;
        px = &WeightMap->values[pic->width*row];
        for (col=Region.x1;col<Region.x2;col++){
            if (px[col]){
                int bv;
                bv = p1[0]+p1[1]*2+p1[2];  // Multiplies by 4.
                brow += bv;
                DetectionPixels += 1;
            }
            p1 += 3;
        }
        baverage += brow;
    }

    if (DetectionPixels < 1000){
        fprintf(stderr, "Detection region too small");
        exit(-1);
    }
   
    return baverage * 0.25 / DetectionPixels; // Multiply by 4 again.
}

//----------------------------------------------------------------------------------------
// Show detection weight map array.
//----------------------------------------------------------------------------------------
static void ShowWeightMap()
{
    int row, width, height;
    width = WeightMap->w;
    height = WeightMap->h;
    printf("Weight map:  '-' = ignore, '1' = normal, '#' = 2x weight\n");

    for (row=0;row<height;row+=4){
        int r;
        printf("   ");
        for (r=0;r<width;r+=4){
            switch (WeightMap->values[row*width+r]){
                case 0: putchar('-');break;
                case 1: putchar('1');break;
                case 2: putchar('#');break;
                default: putchar('?'); break;
            }
        }
        printf("\n");
    }
}

//----------------------------------------------------------------------------------------
// Turn exclude regions into an image map of what to use and not use.
//----------------------------------------------------------------------------------------
void FillWeightMap(int width, int height)
{
    int row, r;
    Region_t Reg;

    WeightMap = malloc(offsetof(ImgMap_t, values) + sizeof(WeightMap->values[0])*width*height);
    WeightMap->w = width; WeightMap->h = height;

    memset(WeightMap->values, 0,  sizeof(WeightMap->values)*width*height);

    Reg = Regions.DetectReg;
    if (Reg.x2 > width) Reg.x2 = width;
    if (Reg.y2 > height) Reg.y2 = height;
    printf("fill %d-%d,%d-%d\n",Reg.x1, Reg.x2, Reg.y1, Reg.y2);
    for (row=Reg.y1;row<Reg.y2;row++){
        memset(&WeightMap->values[row*width+Reg.x1], 1, Reg.x2-Reg.x1);
    }

    for (r=0;r<Regions.NumExcludeReg;r++){
        Reg=Regions.ExcludeReg[r];
        if (Reg.x2 > width) Reg.x2 = width;
        if (Reg.y2 > height) Reg.y2 = height;
        printf("clear %d-%d,%d-%d\n",Reg.x1, Reg.x2, Reg.y1, Reg.y2);
        for (row=Reg.y1;row<Reg.y2;row++){
            memset(&WeightMap->values[row*width+Reg.x1], 0, Reg.x2-Reg.x1);
        }
    }
    
    ShowWeightMap();
}

//----------------------------------------------------------------------------------------
// Load the image to determine which regions to use and which to exclude.
//----------------------------------------------------------------------------------------
void ProcessDiffMap(MemImage_t * MapPic)
{
    int width, height, row, col;
    int firstrow, lastrow;
    int numred, numblue;
    width = MapPic->width;
    height = MapPic->height;

    // Allocate the detection region.
    WeightMap = malloc(offsetof(ImgMap_t, values) 
             + sizeof(DiffVal->values[0])*width*height);
    WeightMap->w = width; 
    WeightMap->h = height;

    numred = numblue = 0;
    firstrow = -1;
    lastrow = 0;

    for (row=0;row<height;row++){
        unsigned char * map;
        unsigned char * img;
        map = &WeightMap->values[width*row];
        img = &MapPic->pixels[width*3*row];
        for (col=0;col<width;col++){
            int r,g,b;
            r = img[0];
            g = img[1];
            b = img[2];
            img += 3;

            // If the map is blue, then ignore.
            // If the map is red, then wight 2x.

            if (b > 100 && b > (r+g)*2){
                // Blue.  Ignore.
                *map = 0;
                numblue += 1;
            }else{
                if (firstrow == -1) firstrow = row;
                lastrow = row;
                *map = 1;                
                if (r > 100 && r > (g+b)*2){
                    // Red.  Weigh 2x.
                    *map = 2;
                    numred += 1;
                }
            }
            map += 1;
        }
    }

    Regions.DetectReg.y1 = firstrow;
    Regions.DetectReg.y2 = lastrow;

    ShowWeightMap();
}

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

    if (DiffVal != NULL && (width != DiffVal->w || height != DiffVal->h)){
        // DiffVal allocated size is wrong.
        free(DiffVal);
        free(WeightMap);
        DiffVal = NULL;
    }
    if (DiffVal == NULL){
        DiffVal = malloc(offsetof(ImgMap_t, values) + sizeof(DiffVal->values[0])*width*height);
        DiffVal->w = width; DiffVal->h = height;
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
        int data_size;
        data_size = height * bPerRow;
        DiffOut = malloc(data_size+offsetof(MemImage_t, pixels));
        memcpy(DiffOut, pic1, offsetof(MemImage_t, pixels));
        memset(DiffOut->pixels, 0, data_size);
    }
    memset(DiffHist, 0, sizeof(DiffHist));

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


    // Compute brightness multipliers.
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
    for (row=MainReg.y1;row<MainReg.y2;){
        unsigned char * p1, *p2, *pd = NULL;
        unsigned char * ExRow;
        unsigned char * diffrow;
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

                
                dr = (p1[0]*m1i - p2[0]*m2i);
                dg = (p1[1]*m1i - p2[1]*m2i);
                db = (p1[2]*m1i - p2[2]*m2i);
                
                if (dr < 0) dr = -dr;
                if (dg < 0) dg = -dg;
                if (db < 0) db = -db;
                dcomp = (dr + dg*2 + db) >> 8; // Put more emphasis on green
                dcomp = dcomp * 120/max; // Adjust for dark parts of image.


                if (dcomp >= 256) dcomp = 255;// put it in a histogram.
                if (dcomp < 0){
                    fprintf(stderr,"Internal error dcomp\n");
                    dcomp = 0;
                }
                DiffHist[dcomp] += 1;
                diffrow[col] = dcomp;
        
                if (DebugImgName){
                    // Save the difference image, scaled 4x.
                    dr=(dr*60/max)>> 6; if (dr > 255) dr = 255;
                    dg=(dg*60/max)>> 6; if (dg > 255) dg = 255;
                    db=(db*60/max)>> 6; if (db > 255) db = 255;
                    pd[0] = dr;
                    pd[1] = dg;
                    pd[2] = db;
                }
            }

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
        // Try to gauge the difference noise (assuming two thirds of the image
        // has not changed)
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
        int maxth = 110 + BrightnessRatio*5;
        if (threshold > maxth) threshold = maxth;

        if (Verbosity) printf("2/3 of image is below %d diff.  Using %d threshold\n",a, threshold);

        // Try to gauge the difference noise (assuming two thirds of the image
        // has not changed)

        Trigger = SearchDiffMaxWindow(MainReg, threshold);
        return Trigger;
    }
}

//----------------------------------------------------------------------------------------
// Search for an N x N window with the maximum differences in it.
// This algorithm is optimized for rejecting spurious differences outdoors
// where grass and leaves can cause a lot of weak spurious motion over large areas.
//----------------------------------------------------------------------------------------
static TriggerInfo_t SearchDiffMaxWindow(Region_t Region, int threshold)
{
    int row,col;
    TriggerInfo_t retval;
    int fs;
        
    // Scale down by this factor before applying windowing algorithm to look for max localized change
    // Thee factors work well with source images that are 1000-2000 pixels across.
    const int scalef = 5;

    // these determine the window over over which to look for the change (after scaling)    
    const int wind_w = 4, wind_h = 7;
    //const int wind_w = 5, wind_h = 8;
    
    static int * DiffScaled = NULL;
    static int * DiffScaledCum = NULL;
    static int * DiffScaledC2 = NULL;
    static int widthSc, heightSc;
    
    static int * Fatigue = NULL;
    if (DiffScaled == NULL){
        #define ROOF(x) ((x+scalef-1)/scalef)
        widthSc = ROOF(DiffVal->w);
        heightSc = ROOF(DiffVal->h);
        DiffScaled = malloc(sizeof(int)*widthSc*heightSc);
        DiffScaledCum = malloc(sizeof(int)*widthSc*heightSc);       
        DiffScaledC2 = malloc(sizeof(int)*widthSc*heightSc);
        Fatigue = malloc(sizeof(int)*widthSc*heightSc);
        memset(Fatigue, 0, sizeof(int)*widthSc*heightSc);
    }else{
        if (widthSc != ROOF(DiffVal->w) || heightSc != ROOF(DiffVal->h)){
            fprintf(stderr, "image size changed, error!");
            // Could reallocate, but probably other code doesn't handle resizing anyway.
            exit(-1);
        }
    }
    
    // Compute scaled down array of differences.  Destination: DiffScaled[]
    {
        int width = DiffVal->w;
        memset(DiffScaled, 0, sizeof(int)*widthSc*heightSc);
        for (row=Region.y1;row<Region.y2;row++){
            // Compute difference by column using established threshold value
            unsigned char * diffrow, *ExRow;
            int * widthScrow;
            diffrow = &DiffVal->values[width*row];
            ExRow = &WeightMap->values[width*row];

            widthScrow = &DiffScaled[widthSc*(row/scalef)];
            for (col=Region.x1;col<Region.x2;col++){
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
    }

    if (Verbosity > 1){
        printf("Scaled difference array (%d x %d)\n", widthSc, heightSc);
        for (row=0;row<heightSc;row++){
            for (col=0;col<widthSc;col++) printf("%3d",DiffScaled[row*widthSc+col]/100);
            printf("\n");
        }
    }

    if (MotionFatigueTc > 0){
        static int SinceFatiguePrint = 0;
        // Compute motion fatigue
        fs = 0;
        for (row=0;row<heightSc;row++){
            for (col=0;col<widthSc;col++){
                int ds, nFat;
                ds = DiffScaled[row*widthSc+col];
                nFat = (Fatigue[row*widthSc+col]*(MotionFatigueTc-1) + ds)/MotionFatigueTc; // Expontential decay on it.
                Fatigue[row*widthSc+col] = nFat;
                fs += nFat;
            }
        }
        fs = fs/(heightSc*widthSc);
        
        // Show fatigue map from time to time.
        if (Verbosity > 1 || (fs > 50 && SinceFatiguePrint > 60)){
            // Print the fatigure array every minuts if there is stuff in it.
            fprintf(Log, "Fatigue map (%d x %d) sum=%d<small>\n", widthSc, heightSc, fs);
            for (row=0;row<heightSc;row++){
                for (col=0;col<widthSc;col++){
                    int v = Fatigue[row*widthSc+col]/50;
                    //                             00112233445566778899
                    static const char LowDigits[20] = " . - = 3 4~5~6~7=8=9";
                    if (v < 10){
                        fprintf(Log,"%c%c",LowDigits[v*2],LowDigits[v*2+1]);
                    }else{
                        fprintf(Log,"%2d",v <= 99 ? v : 99);
                    }
                }
                fprintf(Log,"\n");
            }
            fprintf(Log, "</small>\n");
            SinceFatiguePrint = 0;
        }
        SinceFatiguePrint++;
    

        // Subtract out motion fatigue
        for (row=0;row<heightSc;row++){
            for (col=0;col<widthSc;col++){
                int ds, nFatM;
                nFatM = Fatigue[row*widthSc+col];
                // Look for largest fatigue also among largest 4 neighbours to use.
                // Helps address laundry flapping in the wind, sometimes flapping further.
                if (col > 0         && nFatM < Fatigue[row*widthSc+col-1]) nFatM = Fatigue[row*widthSc+col-1];
                if (col < widthSc   && nFatM < Fatigue[row*widthSc+col+1]) nFatM = Fatigue[row*widthSc+col+1];
                if (row > 0         && nFatM < Fatigue[(row-1)*widthSc+col]) nFatM = Fatigue[(row-1)*widthSc+col];
                if (row <heightSc-1 && nFatM < Fatigue[(row+1)*widthSc+col]) nFatM = Fatigue[(row+1)*widthSc+col];
                
                ds = DiffScaled[row*widthSc+col];
                ds -= nFatM*3;
                if (ds < 0) ds = 0;
                DiffScaled[row*widthSc+col] = ds;
            }
        }
    }

    // Transform array to column sums wind_h above.  DiffScaled[] --> DiffScaledCum[]
    memset(DiffScaledCum, 0, sizeof(int)*widthSc*heightSc);
    for (row=0;row<heightSc;row++){
        int *oldrow, *newrow, *addrow;
        oldrow = &DiffScaledCum[widthSc*(row-1)];
        newrow = &DiffScaledCum[widthSc*row];
        
        addrow = &DiffScaled[widthSc*row];
        if (row >= wind_h){
            int * subtrow = &DiffScaled[widthSc*(row-wind_h)];
            for (col=0;col<widthSc;col++){
                newrow[col] = addrow[col]+oldrow[col]-subtrow[col];
            }
        }else{
            if (row == 0){
                for (col=0;col<widthSc;col++) newrow[col] = addrow[col];
            }else{
                for (col=0;col<widthSc;col++){
                    newrow[col] = addrow[col]+oldrow[col];
                }
            }
        }
    }

    // Transform array to sum of wind_w to the left.  DiffScaledCum[] --> DiffScaledC2
    {
        int maxval, maxr, maxc;
        maxval = maxr = maxc = 0;
        // Transform array to sum of n columns from previous.
        for (row=0;row<heightSc;row++){
            int *srcrow, *destrow;
            int s;
            srcrow = &DiffScaledCum[widthSc*row];
            destrow = &DiffScaledC2[widthSc*row];
            s = 0;
            for (col=0;col<widthSc;col++){
                s += srcrow[col];
                if (col >= wind_w) s-= srcrow[col-wind_w];
                destrow[col] = s;
                if (s > maxval){
                    maxval = s;
                    maxc = col;
                    maxr = row;
                }
            }
        }
        if (Verbosity) printf("window max v=%d at col=%d,row=%d\n",maxval/100, maxc, maxr);
        retval.x = maxc * scalef - wind_w * scalef / 2;
        retval.y = maxr * scalef - wind_h * scalef / 2;
        if (retval.x < 0) retval.x = 0;
        if (retval.y < 0) retval.y = 0;
        retval.DiffLevel = retval.Motion = maxval / 100;

        row = maxr-wind_h+1;
        if (row < 0) row = 0;
        if (Verbosity) printf("Window contents.  Cols %d-%d Rows %d-%d\n",maxc-wind_w+1,maxc,row,maxr);
        if (maxval > 0){
            double xsum, ysum;
            int sum;
            xsum = ysum = sum = 0;
            for (;row<=maxr;row++){
                col = maxc-wind_w+1; 
                if (col < 0) col = 0;
                for(;col<=maxc;col++){
                    int v = DiffScaled[row*widthSc+col];
                    xsum += col*v;
                    ysum += row*v;
                    sum += v;
                    //printf("%3d",DiffScaled[row*widthSc+col]/100);
                }
                //printf("\n");
            }
            if (Verbosity) printf("Exact r,c= col=%5.1f, row=%5.1f\n",xsum*1.0/sum, ysum*1.0/sum);
            retval.x = (int)(xsum*scalef*ScaleDenom/sum)+scalef*ScaleDenom/2;
            retval.y = (int)(ysum*scalef*ScaleDenom/sum)+scalef*ScaleDenom/2;
            if (Verbosity) printf("Picture coordinates: x,y = %d,%d\n",retval.x, retval.y);
        }
    }
   
    if (Verbosity > 2){ 
        // Show the array.
        printf("Window sums\n");
        for (row=0;row<heightSc;row++){
            for (col=0;col<widthSc;col++) printf("%3d",DiffScaledC2[row*widthSc+col]/100);
            printf("\n");
        }
    }
    
    return retval;
}

