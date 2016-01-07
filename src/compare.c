//-----------------------------------------------------------------------------------
// imgcomp image comparison module
// Matthias Wandel 2015
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

int NewestAverageBright;
int NightMode;

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
    double baverage;
    int DetectionPixels;
    int row;
    int rowbytes = pic->width*3;
    DetectionPixels = 0;
 
    // Compute average brightnesses.
    baverage = 0;
    for (row=Region.y1;row<Region.y2;row++){
        unsigned char *p1, *px;
        int col, brow = 0;
        p1 = pic->pixels+rowbytes*row;
        px = &WeightMap->values[pic->width*row];
        for (col=Region.x1;col<Region.x2;col++){
            if (px[col]){
                brow += p1[0]+p1[1]*2+p1[2];  // Muliplies by 4.
                p1 += 3;
                DetectionPixels += 1;
            }
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
    Region_t MainReg;
    TriggerInfo_t RetVal;
    RetVal.x = RetVal.y = 0;
    RetVal.DiffLevel = -1;

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

        NightMode = 0;
        if (b1average < 15 || b2average < 15){
            if (Verbosity > 0) printf("Using night mode diff\n");
            NightMode = 1;
        }

        m1 = 80/b1average;
        m2 = 80/b2average;

        {
            double maxm = m1 > m2 ? m1 : m2;
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

        if (!NightMode || b2average > b1average){
            m1i = (int)(m1*256+0.5);
            m2i = (int)(m2*256+0.5);
        }else{
            // Swap images and multipliers so that pic2 is the brighter one.
            // this makes giving it slack easier.
            MemImage_t * temp;
            temp = pic1; pic1=pic2; pic2=temp;
            m1i = (int)(m2*256+0.5);  // m1 should always be bigger.
            m2i = (int)(m1*256+0.5);
            if (Verbosity) printf("swapped images m1i=%d m2i=%d\n",m1i,m2i);
        }
    }

    // Compute differences
    for (row=MainReg.y1;row<MainReg.y2;){
        unsigned char * p1, *p2, *pd = NULL;
        unsigned char * ExRow;
        unsigned char * diffrow;
        p1 = pic1->pixels+row*bPerRow;
        p2 = pic2->pixels+row*bPerRow;
        diffrow = &DiffVal->values[width*row];
        ExRow = &WeightMap->values[width*row];
        if (DebugImgName) pd = DiffOut->pixels+row*bPerRow;

        if (NightMode){
            for (col=MainReg.x1;col<MainReg.x2;col+= 2){
                unsigned char * pn;
                int b1, b2, dcomp;
                if (ExRow[col]){
                    // Add up four pixels for noise reduction.
                    b1 = (p1[0]+p1[1]*2+p1[2] + p1[3]+p1[4]*2+p1[5]);
                    pn = p1+bPerRow;
                    b1 += (pn[0]+pn[1]*2+pn[2] + pn[3]+pn[4]*2+pn[5]);

                    b2 = (p2[0]+p2[1]*2+p2[2] + p2[3]+p2[4]*2+p2[5]);
                    pn = p2+bPerRow;
                    b2 += (pn[0]+pn[1]*2+pn[2] + pn[3]+pn[4]*2+pn[5]);

                    dcomp = b2*m2i-b1*m1i; // Differentce with ratio applied.

                    if (dcomp < 0){
                        // difference might be on account of m1i > m2i.
                        // Try it without multiplication difference.
                        dcomp = b2*m1i-b1*m1i;
                        if (dcomp > 0){
                            // if difference now positive, the whole difference seen may
                            // be because m1i > m2i.  So call it zero.
                            dcomp = 0;
                        }
                    }

                    dcomp = dcomp >> 9;

                    if (DebugImgName){
                        pd[1] = pd[4] = dcomp > 0 ? dcomp : 0; // Green = img2 brighter
                        pd[0] = pd[3] = dcomp < 0 ? -dcomp : 0; // Red = img1 brigher.
                        pd[2] = pd[5] = 0;
                    }

                    if (dcomp < 0) dcomp = -dcomp;
                    if (dcomp > 255) dcomp = 255;// put it in a histogram.
                    DiffHist[dcomp] += 4; // Did four pixels, so add 4.
                    diffrow[col] = dcomp;
                }

                pd += 6;
                p1 += 6;
                p2 += 6;
            }
            row+=2;
        }else{
            for (col=MainReg.x1;col<MainReg.x2;col++){
                if (ExRow[col]){
                    // Data is in order red, green, blue.
                    int dr,dg,db, dcomp;
                    dr = (p1[0]*m1i - p2[0]*m2i);
                    dg = (p1[1]*m1i - p2[1]*m2i);
                    db = (p1[2]*m1i - p2[2]*m2i);

                    if (dr < 0) dr = -dr;
                    if (dg < 0) dg = -dg;
                    if (db < 0) db = -db;
                    dcomp = (dr + dg*2 + db);     // Put more emphasis on green
                    dcomp = dcomp >> 8;           // Remove the *256 from fixed point aritmetic multiply

                    if (dcomp >= 256) dcomp = 255;// put it in a histogram.
                    DiffHist[dcomp] += 1;
                    diffrow[col] = dcomp;

            
                    if (DebugImgName){
                        // Save the difference image, scaled 4x.
                        if (dr > 256) dr = 256;
                        if (dg > 256) dg = 256;
                        if (db > 256) db = 256;
                        pd[0] = dr;
                        pd[1] = dg;
                        pd[2] = db;;
                    }
                }

                pd += 3;
                p1 += 3;
                p2 += 3;
            }
            row++;
        }
    }

    if (DebugImgName){
        WritePpmFile(DebugImgName,DiffOut);
        free(DiffOut);
    }

    

    if (Verbosity > 1){
        for (a=0;a<60;a+= 2){
            printf("%3d:%5d,%5d\n",a,DiffHist[a], DiffHist[a+1]);
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
        if (threshold > 80) threshold = 80;

        if (Verbosity) printf("2/3 of image is below %d diff.  Using %d threshold\n",a, threshold);
        
        cumsum = 0;
        for (a=threshold;a<256;a++){
            cumsum += DiffHist[a] * (a-threshold);
        }
        if (Verbosity) printf("Above threshold: %d\n",cumsum);

        cumsum = (cumsum * 100) / DetectionPixels;
        if (Verbosity) printf("Normalized diff: %d\n",cumsum);

        // Try to gauge the difference noise (assuming two thirds of the image
        // has not changed)

        Trigger = SearchDiffMaxWindow(MainReg, threshold);
        return Trigger;
    }
}

//----------------------------------------------------------------------------------------
// Search for an N x N window with the maximum differences in it.
//----------------------------------------------------------------------------------------
static TriggerInfo_t SearchDiffMaxWindow(Region_t Region, int threshold)
{
    int row,col;
    TriggerInfo_t retval;
    
    // Scale down by this factor before applying windowing algorithm to look for max localized change
    #define SCALEF 4
    #define ROOF(x) ((x+SCALEF-1)/SCALEF)

    // these determine the window over over which to look for the change (after scaling)    
    const int wind_w = 4, wind_h = 4;
    
    static int * Diff4 = NULL;
    static int * Diff4b = NULL;
    static int width4, height4;
    if (width4 != ROOF(DiffVal->w) || height4 != ROOF(DiffVal->h) || Diff4 == NULL){
        // Size has changed.  Reallocate.
        free(Diff4);
        width4 = ROOF(DiffVal->w);
        height4 = ROOF(DiffVal->h);
        Diff4 = malloc(sizeof(int)*width4*height4);
        Diff4b = malloc(sizeof(int)*width4*height4);        
    }

    // Compute scaled down array of differences.
    {
        int width = DiffVal->w;
        memset(Diff4, 0, sizeof(int)*width4*height4);
        for (row=Region.y1;row<Region.y2;row++){
            // Compute difference by column using established threshold value
            unsigned char * diffrow, *ExRow;
            int * width4row;
            diffrow = &DiffVal->values[width*row];
            ExRow = &WeightMap->values[width*row];

            width4row = &Diff4[width4*(row/SCALEF)];
            for (col=Region.x1;col<Region.x2;col++){
                int d = diffrow[col] - threshold;
                if (d > 0){
                    width4row[col/SCALEF] += d;
                    if (ExRow[col] > 1){
                        // Double weight region
                        width4row[col/SCALEF] += d;
                    }
                }
            }
        }
    }

    if (Verbosity > 1){
        // Show the array.
        for (row=0;row<height4;row++){
            for (col=0;col<width4;col++) printf("%3d",Diff4[row*width4+col]/100);
            printf("\n");
        }
    }
    
    // Transform array to sum of wind_h rows
    memset(Diff4b, 0, sizeof(int)*width4*height4);
    for (row=0;row<height4;row++){
        int *oldrow, *newrow, *addrow;
        oldrow = &Diff4b[width4*(row-1)];
        newrow = &Diff4b[width4*row];
        
        addrow = &Diff4[width4*row];
        if (row >= wind_h){
            int * subtrow = &Diff4[width4*(row-wind_h)];
            for (col=0;col<width4;col++){
                newrow[col] = addrow[col]+oldrow[col]-subtrow[col];
            }
        }else{
            if (row == 0){
                for (col=0;col<width4;col++) newrow[col] = addrow[col];
            }else{
                for (col=0;col<width4;col++){
                    newrow[col] = addrow[col]+oldrow[col];
                }
            }
        }
    }

    // Transform array to sum of wind_w rows, while also looking for maximum.
    {
        int maxval, maxr, maxc;
        maxval = maxr = maxc = 0;
        // Transform array to sum of n columns from previous.
        for (row=0;row<height4;row++){
            int *srcrow, *destrow;
            int s;
            srcrow = &Diff4b[width4*row];
            destrow = &Diff4[width4*row];
            s = 0;
            for (col=0;col<width4;col++){
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
        if (Verbosity) printf("max v=%d at r=%d,c=%d\n",maxval/100, maxr, maxc);
        retval.x = maxc * SCALEF - wind_w * SCALEF / 2;
        retval.y = maxr * SCALEF - wind_h * SCALEF / 2;
        if (retval.x < 0) retval.x = 0;
        if (retval.y < 0) retval.y = 0;
        retval.DiffLevel = maxval / 100;
    }
   
    if (Verbosity > 1){ 
        // Show the array.
        printf("Window sums\n");
        for (row=0;row<height4;row++){
            for (col=0;col<width4;col++) printf("%3d",Diff4[row*width4+col]/100);
            printf("\n");
        }
    }
    
    return retval;
}


