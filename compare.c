#include <stdio.h>
#include <stdio.h>
#include <malloc.h>
#include <stddef.h>
#include <memory.h>
#include "imgcomp.h"

int NewestAverageBright;

//----------------------------------------------------------------------------------------
// Compare two images in memory
//----------------------------------------------------------------------------------------
int ComparePix(MemImage_t * pic1, MemImage_t * pic2, Region_t Region, char * DebugImgName, int Verbosity)
{
    int width, height, comp;
    int row, col;
    MemImage_t * DiffOut;
    int DiffHist[256];
    int a;
    int DetectionPixels;
    int b1average, b2average;
    int m1, m2;

    if (Verbosity){
        printf("\ncompare pictures %dx%d %d\n", pic1->width, pic1->height, pic1->components);
    }
    
    if (pic1->width != pic2->width || pic1->height != pic2->height 
        || pic1->components != pic2->components){
        fprintf(stderr, "pic types mismatch!\n");
        return -1;
    }

    width = pic1->width;
    height = pic1->height;
    comp = pic1->components;

    if (DebugImgName){
        int data_size;
        data_size = width * height * comp;
        DiffOut = malloc(data_size+offsetof(MemImage_t, pixels));
        memcpy(DiffOut, pic1, offsetof(MemImage_t, pixels));
        memset(DiffOut->pixels, 0, data_size);
    }
    memset(DiffHist, 0, sizeof(DiffHist));

    if (Region.y2 > height) Region.y2 = height;
    if (Region.x2 > width) Region.x2 = width;
    if (Region.x2 < Region.x1 || Region.x2 < Region.x1){
        fprintf(stderr, "Negative region, or region outside of image\n");
        return -1;
    }
    DetectionPixels = (Region.x2-Region.x1) * (Region.y2-Region.y1);
    if (DetectionPixels < 1000){
        fprintf(stderr, "Too few pixels in region\n");
        return -1;
    }

    if (Verbosity > 0){
        printf("Detection region is %d-%d, %d-%d\n",Region.x1, Region.x2, Region.y1, Region.y2);
    }

    // Compute average brightnesses.
    b1average = b2average = 0;
    for (row=Region.y1;row<Region.y2;row++){
        unsigned char * p1, *p2;
        int b1row, b2row;
        p1 = pic1->pixels+width*comp*row+Region.x1*comp;
        p2 = pic2->pixels+width*comp*row+Region.x1*comp;
        b1row = b2row = 0;
        for (col=Region.x1;col<Region.x2;col++){
            b1row += p1[0]+p1[1]*2+p1[2];
            b2row += p2[0]+p2[1]*2+p2[2];
        }
        b1average += (b1row >> 4);
        b2average += (b2row >> 4);
    }
    b1average = b1average / (DetectionPixels >> 4);
    b2average = b2average / (DetectionPixels >> 4);
    NewestAverageBright = b2average / 4;
    if (Verbosity > 0){
        printf("average bright: %d %d\n",b1average/4, b2average/4);
    }
    if (b1average == 1) b1average = 1; // Avoid possible division by zero.
    if (b2average == 1) b2average = 1;
    m1 = 256*110*4/b1average;
    m2 = 256*110*4/b2average;
    {
        // Don't allow multiplier to get bigger than 2.  Otherwise, for dark images
        // we just end up multiplying pixel noise!
        int mm = m1 > m2 ? m1 : m2;
        if (mm > 256*2){
            m1 = m1 * (256*2) / mm;
            m2 = m2 * (256*2) / mm;
        }
    }

    if (Verbosity > 0){
        printf("Brightness adjust multipliers:  m1 = %5.2f  m2=%5.2f\n",m1/256.,m2/256.);
    }

    // Compute differences
    for (row=Region.y1;row<Region.y2;row++){
        unsigned char * p1, *p2, *pd;
        p1 = pic1->pixels+width*comp*row+Region.x1*comp;
        p2 = pic2->pixels+width*comp*row+Region.x1*comp;

        pd = NULL;
        if (DebugImgName){
            pd = DiffOut->pixels+width*comp*row+Region.x1*comp;
        }

        for (col=Region.x1;col<Region.x2;col++){
            // Data is in order red, green, blue.
            int dr,dg,db, dcomp;
            dr = (p1[0]*m1 - p2[0]*m2);
            dg = (p1[1]*m1 - p2[1]*m2);
            db = (p1[2]*m1 - p2[2]*m2);

            if (dr < 0) dr = -dr;
            if (dg < 0) dg = -dg;
            if (db < 0) db = -db;
            dcomp = (dr + dg*2 + db);     // Put more emphasis on green
            dcomp = dcomp >> 8;           // Remove the *256 from fixed point aritmetic multiply

            if (dcomp >= 256) dcomp = 255;// put it in a histogram.
            DiffHist[dcomp] += 1;
            
            if (DebugImgName){
                // Save the difference image, scaled 4x.
                if (dr > 63) dr = 63;
                if (dg > 63) dg = 63;
                if (db > 63) db = 63;
                pd[0] = dr*4;
                pd[1] = dg*4;
                pd[2] = db*4;
                pd += comp;
            }

            p1 += comp;
            p2 += comp;
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
        int cumsum = 0;
        int threshold;

        for (a=0;a<256;a++){
            cumsum += DiffHist[a];
            if (cumsum >= DetectionPixels/2) break;
        }
        if (Verbosity) printf("half of image is below %d diff\n",a);

//printf("half of image is below %d diff %d %d\n",a, b1average, b2average);
        threshold = a*4+2;
        
        cumsum = 0;
        for (a=threshold;a<256;a++){
            cumsum += DiffHist[a] * (a-threshold);
        }
        if (Verbosity) printf("Above threshold: %d pixels\n",cumsum);

        cumsum = (cumsum * 100) / DetectionPixels;
        if (Verbosity) printf("Normalized diff: %d\n",cumsum);
        return cumsum;
    }
}