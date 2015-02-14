#include <stdio.h>
#include <stdio.h>
#include <malloc.h>
#include <stddef.h>
#include <memory.h>
#include "imgcomp.h"

int NewestAverageBright;
int NightMode;

//----------------------------------------------------------------------------------------
// Calculate average brightness of an image.
//----------------------------------------------------------------------------------------
double AverageBright(MemImage_t * pic, Region_t Region)
{
    double baverage;
    int DetectionPixels;
    int row;
    int rowbytes = pic->width*3;
    DetectionPixels = (Region.x2-Region.x1) * (Region.y2-Region.y1);
 
    // Compute average brightnesses.
    baverage = 0;
    for (row=Region.y1;row<Region.y2;row++){
        unsigned char * p1;
        int col, brow = 0;
        p1 = pic->pixels+rowbytes*row;
        for (col=Region.x1;col<Region.x2;col++){
            brow += p1[0]+p1[1]*2+p1[2];  // Muliplies by 4.
            p1 += 3;
        }
        baverage += brow; 
    }
    return baverage * 0.25 / DetectionPixels; // Multiply by 4 again.
}


//----------------------------------------------------------------------------------------
// Compare two images in memory
//----------------------------------------------------------------------------------------
int ComparePix(MemImage_t * pic1, MemImage_t * pic2, Region_t Region, char * DebugImgName, int Verbosity)
{
    int width, height, bPerRow;
    int row, col;
    MemImage_t * DiffOut = NULL;
    int DiffHist[256];
    int a;
    int DetectionPixels;
    int m1i, m2i;

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
    bPerRow = width * 3;

    if (DebugImgName){
        int data_size;
        data_size = height * bPerRow;
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
    {
        double b1average, b2average;
        double m1, m2;
        b1average = AverageBright(pic1, Region);
        b2average = AverageBright(pic2, Region);

        NewestAverageBright = (int)(b2average+0.5);

        if (Verbosity > 0){
            printf("average bright: %f %f\n",b1average, b2average);
        }
        if (b1average < 1) b1average = 1; // Avoid possible division by zero.
        if (b2average < 1) b2average = 1;

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
    for (row=Region.y1;row<Region.y2;){
        unsigned char * p1, *p2, *pd = NULL;
        p1 = pic1->pixels+row*bPerRow;
        p2 = pic2->pixels+row*bPerRow;
        if (DebugImgName) pd = DiffOut->pixels+row*bPerRow;

        if (NightMode){
            for (col=Region.x1;col<Region.x2;col+= 2){
                unsigned char * pn;
                int b1, b2, dcomp;
                // Add up two pixels for noise reduction.
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
                    dcomp = b1*m1i-b2*m1i;
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
                    pd += 6;
                }

                if (dcomp < 0) dcomp = -dcomp;
                if (dcomp >= 256) dcomp = 255;// put it in a histogram.
                DiffHist[dcomp] += 4; // Did four pixels, so add 4.

                p1 += 6;
                p2 += 6;
            }
            row+=2;
        }else{
            for (col=Region.x1;col<Region.x2;col++){
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
            
                if (DebugImgName){
                    // Save the difference image, scaled 4x.
                    if (dr > 256) dr = 256;
                    if (dg > 256) dg = 256;
                    if (db > 256) db = 256;
                    pd[0] = dr;
                    pd[1] = dg;
                    pd[2] = db;;
                    pd += 3;
                }

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
        int cumsum = 0;
        int threshold;
        int twothirds = DetectionPixels*2/3;

        for (a=0;a<256;a++){
            if (cumsum >= twothirds) break;
            cumsum += DiffHist[a];
        }
        if (Verbosity) printf("2/3 of image is below %d diff\n",a);

//printf("half of image is below %d diff %d %d\n",a, b1average, b2average);
        threshold = a*3+4;
        if (threshold < 20) threshold = 20;
        if (threshold > 70) threshold = 70;
        
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
