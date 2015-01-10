#include <stdio.h>
#include <stdio.h>
#include <malloc.h>
#include <stddef.h>
#include <memory.h>
#include "imgcomp.h"

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

    // todo: scale brightness.

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

    // Compute differences
    if (Verbosity > 0){
        printf("Detection region is %d-%d, %d-%d\n",Region.x1, Region.x2, Region.y1, Region.y2);
    }

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
            dr = p1[0] - p2[0];
            dg = p1[1] - p2[1];
            db = p1[2] - p2[2];

            if (dr < 0) dr = -dr;
            if (dg < 0) dg = -dg;
            if (db < 0) db = -db;
            dcomp = (dr * 2 + dg*4 + db * 2) >> 3; // Put more emphasis on green
            if (dcomp >= 256) dcomp = 255;

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
        for (a=0;a<30;a++){
            printf("%3d: %d\n",a,DiffHist[a]);
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
        threshold = a*3+10;
        
        cumsum = 0;
        for (a=threshold;a<256;a++){
            if (a < threshold+10 && Verbosity > 1) printf("hist %d %d\n",a,DiffHist[a]);
            cumsum += DiffHist[a] * (a-threshold);
        }
        if (Verbosity) printf("Above threshold: %d pixels\n",cumsum);

        cumsum = (cumsum * 100) / DetectionPixels;
        if (Verbosity) printf("Normalized diff: %d\n",cumsum);
        return cumsum;
    }
}