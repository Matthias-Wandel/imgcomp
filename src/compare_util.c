//-----------------------------------------------------------------------------------
// Comparison module helper functions
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

//#define Log stdout
//#if 0
//----------------------------------------------------------------------------------------
// Show detection weight map array.
//----------------------------------------------------------------------------------------
static void ShowWeightMap(ImgMap_t * WeightMap)
{
    int row, width, height;
    width = WeightMap->w;
    height = WeightMap->h;
    printf("Weight map (%dx%d):  '-' = ignore, '1' = normal, '#' = 2x weight\n",width, height);
	int skip = width/100;
	if (skip < 4) skip = 4;
    for (row=skip/2;row<height;row+=skip){
        int r;
        printf("  ");
        for (r=skip/2;r<width;r+=skip){
            switch (WeightMap->values[row*width+r]){
                case 0: putchar('-');break;
                case 1: putchar('1');break;
                case 2: putchar('#');break;
                default: 
				printf(" %d",WeightMap->values[row*width+r]);
				putchar('?'); break;
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

    WeightMap = malloc(offsetof(ImgMap_t, values) + sizeof(WeightMap->values[0])*width*height); // fixme - use init function
    WeightMap->w = width; WeightMap->h = height;

    memset(WeightMap->values, 0,  sizeof(WeightMap->values)*width*height);

    Reg = Regions.DetectReg;
    if (Reg.x2 > width) Reg.x2 = width;
    if (Reg.y2 > height) Reg.y2 = height;
    printf("fill %d-%d,%d-%d\n",Reg.x1, Reg.x2, Reg.y1, Reg.y2);
    for (row=Reg.y1;row<Reg.y2;row++){
		int * rowP = &WeightMap->values[row*width];
		for (int col=Reg.x1;col<Reg.x2;col++){
			rowP[col] = 1;
		}
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
    
    ShowWeightMap(WeightMap);
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
             + sizeof(WeightMap->values[0])*width*height);
    WeightMap->w = width; 
    WeightMap->h = height;

    numred = numblue = 0;
    firstrow = -1;
    lastrow = 0;

    for (row=0;row<height;row++){
        int * map;
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

    ShowWeightMap(WeightMap);
}

//----------------------------------------------------------------------------------------
// Calculate average brightness of an image.
//----------------------------------------------------------------------------------------
double AverageBright(MemImage_t * pic, Region_t Region, ImgMap_t* WeightMap)
{
    double baverage;//
    int DetectionPixels;
    int row;
    int rowbytes = pic->width*3;
    DetectionPixels = 0;
 
    // Compute average brightnesses.
    baverage = 0;//rzaverage = 0;
    for (row=Region.y1;row<Region.y2;row++){
        unsigned char *p1;
		int *px;
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

//#endif























//----------------------------------------------------------------------------------------
// Initialize an empty image map
//----------------------------------------------------------------------------------------
ImgMap_t * MakeImgMap(int w,int h)
{
	ImgMap_t * ImgMap = malloc(sizeof(int)*w*h+sizeof(ImgMap_t));
	ImgMap->w = w;
	ImgMap->h = h;
	return ImgMap;
}


//----------------------------------------------------------------------------------------
// Show image map
//----------------------------------------------------------------------------------------
void ShowImgMap(ImgMap_t * map, int divisor)
{
	int w = map->w;
    fprintf(Log," ");
	for (int c=0;c<map->w;c++) fprintf(Log,"%2d",c%10);
	fprintf(Log,"\n");
	for (int r=0;r<map->h;r++){
		fprintf(Log,"%d",r%10 == 0 ? '=' : '|');
		for (int c=0;c<map->w;c++){
			const char LowDigits[20] = "   - = 3 4~5~6~7=8=9";
			int v = map->values[r*w+c]/divisor;
            if (v < 10){
                fprintf(Log,"%c%c",LowDigits[v*2],LowDigits[v*2+1]);
            }else{
                fprintf(Log,"%2d",v <= 99 ? v : 99);
            }
		}
		fprintf(Log,"%c\n",r%10 == 0 ? '=' : '|');
	}
}


//----------------------------------------------------------------------------------------
// Make each element the maximum of it's neighbours (for growing the fatigue map)
//----------------------------------------------------------------------------------------
void BloomImgMap(ImgMap_t * src, ImgMap_t * dst)
{
	int w = src->w;
	int h = src->h;

	// Bloom vertically while copying to dst array
	for (int r=0;r<h;r++){
		int * row = src->values+src->w*r;
		int * prevrow, *nextrow;
		prevrow = src->values+src->w*(r-1);
		if (r ==0) prevrow = src->values;
		
		nextrow = src->values+src->w*(r+1);
		if (r >= h-1) nextrow = row;

		int * dstrow = dst->values+src->w*r;
		
		for (int c=0;c<w;c++){
			int nv = row[c];
			if (nv < prevrow[c]) nv = prevrow[c];
			if (nv < nextrow[c]) nv = nextrow[c];
			dstrow[c] = nv;
		}
	}

	// Bloom horizontally in place
	for (int r=0;r<h;r++){
		int * row = dst->values+src->w*r;
		
		int pv = 0;
		int c;
		for (c=0;c<w-1;c++){
			int nv = row[c];
			if (nv < pv) nv = pv;
			if (row[c+1]>nv) nv = row[c+1];
			pv = row[c];
			row[c] = nv;
		}
		if (row[c] < pv) row[c] = pv;
	}

}

//----------------------------------------------------------------------------------------
// Block filter an image.  Modifies in place.
//----------------------------------------------------------------------------------------
int BlockFilterImgMap(ImgMap_t * src, ImgMap_t * dst, int fw, int fh, int * pmaxc, int * pmaxr)
{
	int w = src->w;
	int h = src->h;
	if (fw > w || fh > h){
		fprintf(stderr, "filter too big\n");
		return 0;
	}

	// Sum horizontally and copy to dst.
	fw -= 1;
	for (int r=0;r<h;r++){
		int sum=0;
		int * row = &src->values[r*w];
		int * dstrow = &dst->values[r*w];
		int c;
		for (c=0;c<fw;c++){
			sum += row[c];
		}
		for (c=0;c<w-fw;c++){
			int ov = row[c];
			sum += row[c+fw];
			dstrow[c] = sum;
			sum -= ov;
		}
		for (;c<w;c++) dstrow[c] = 0; // Clear unused columns.
	}

	int maxr, maxc, maxv; // For maximum search while doing columns.
	maxr = maxc = maxv = 0;
	
	// Sum vertically.
	fh -= 1;
	int sums[w];
	memset(sums, 0, sizeof(sums));
	for (int r=0;r<fh;r++){
		int * row = &dst->values[r*w];
		for (int c=0;c<w;c++){
			sums[c] += row[c];
		}
	}
	for (int r=0;r<h-fh;r++){
		int * row = &dst->values[r*w];
		int * rowp = &dst->values[(r+fh)*w];
		for (int c=0;c<w;c++){
			int ov = row[c];
			int sum = sums[c]+rowp[c];
			row[c] = sum;
			if (sum > maxv){
				maxv = sum;
				maxr = r;
				maxc = c;
			}
			sums[c] = sum-ov;
		}
	}
	// Clear out unused rows.
	memset(&dst->values[(h-fh)*w],0,sizeof(dst->values[0])*w*fh);
	
	
	// Return location of peak
	//printf("max of %d found at %d,%d\n",maxv,maxc,maxr);
	if (pmaxr) *pmaxr = maxr;
	if (pmaxc) *pmaxc = maxc;
	return maxv;
}



/*
int main()
{
	printf("hello\n");
	
	ImgMap_t * Test = MakeImgMap(30,15);
	ImgMap_t * TestD = MakeImgMap(30,15);
	Test->values[Test->w*2+1] = 1;
	Test->values[Test->w*6+6] = 3;
	Test->values[Test->w*11+26] = 4;
	Test->values[Test->w*12+27] = 4;
	Test->values[Test->w*13+28] = 4;
	Test->values[Test->w*14+29] = 4;
	ShowImgMap(Test,1);

	BloomImgMap(Test, TestD);
	printf("Bloomed map:\n");
	ShowImgMap(TestD,1);

	BlockFilterImgMap(TestD, 3,4, NULL, NULL, NULL);
	printf("Block filtered map:\n");
	ShowImgMap(TestD,1);
	
}
*/