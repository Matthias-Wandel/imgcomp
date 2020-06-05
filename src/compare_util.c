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

int NewestAverageBright;


typedef struct {
    int w, h;
    int values[0];
}ImgMap_t;

#define Log stdout




void ShowImgMap(ImgMap_t * map, int divisor)
{
	int w = map->w;
	for (int r=0;r<map->h;r++){
		for (int c=0;c<map->w;c++){
			const char LowDigits[20] = " . - = 3 4~5~6~7=8=9";
			int v = map->values[r*w+c]/divisor;
            if (v < 10){
                fprintf(Log,"%c%c",LowDigits[v*2],LowDigits[v*2+1]);
            }else{
                fprintf(Log,"%2d",v <= 99 ? v : 99);
            }
		}
		printf("\n");
	}
}

ImgMap_t * MakeIntMap(int w,int h)
{
	ImgMap_t * ImgMap = malloc(sizeof(int)*w*h+sizeof(ImgMap_t));
	ImgMap->w = w;
	ImgMap->h = h;
	return ImgMap;
}

//----------------------------------------------------------------------------------------
// Make each element the maximum of it's neighbours (for growing the fatigue map)
//----------------------------------------------------------------------------------------
void BloomIntMap(ImgMap_t * src)
{
	int w = src->w;
	int h = src->h;
	
	// Bloom horizontally
	for (int r=0;r<h;r++){
		int * row = src->values+src->w*r;
		
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
	
	// Bloom vertically
	int prevrow[w];
	memset(prevrow, 0, sizeof(prevrow));

	int * nextrow;
	for (int r=0;r<h;r++){
		int * row = src->values+src->w*r;
		if (r < h-1) nextrow = src->values+src->w*(r+1);
		
		for (int c=0;c<w;c++){
			int nv = row[c];
			if (nv < prevrow[c]) nv = prevrow[c];
			if (nv < nextrow[c]) nv = nextrow[c];
			prevrow[c] = row[c];
			row[c] = nv;
		}
	}
}

//----------------------------------------------------------------------------------------
// Block filter an image.  Modifies in place.
//----------------------------------------------------------------------------------------
void BlockFilterIntMap(ImgMap_t * src, int fw, int fh, int * pmaxv, int * pmaxr, int * pmaxc)
{
	int w = src->w;
	int h = src->h;
	if (fw > w || fh > h){
		fprintf(stderr, "filter too big\n");
		return;
	}

	// Sum horizontally.
	fw -= 1;
	for (int r=0;r<h;r++){
		int sum=0;
		int * row = &src->values[r*w];
		int c;
		for (c=0;c<fw;c++){
			sum += row[c];
		}
		for (c=0;c<w-fw;c++){
			int ov = row[c];
			sum += row[c+fw];
			row[c] = sum;
			sum -= ov;
		}
		for (;c<w;c++) row[c] = 0; // Clear unused columns.
	}

	int maxr, maxc, maxv; // For maximum search.
	maxr = maxc = maxv = 0;
	
	// Sum vertically.
	fh -= 1;
	int sums[w];
	memset(sums, 0, sizeof(sums));
	for (int r=0;r<fh;r++){
		int * row = &src->values[r*w];
		for (int c=0;c<w;c++){
			sums[c] += row[c];
		}
	}
	for (int r=0;r<h-fh;r++){
		int * row = &src->values[r*w];
		int * rowp = &src->values[(r+fh)*w];
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
	memset(&src->values[(h-fh)*w],0,sizeof(src->values[0])*w*fh);
	
	printf("max of %d found at %d,%d\n",maxv,maxc,maxr);
	if (pmaxv) *pmaxv = maxv;
	if (pmaxr) *pmaxv = maxr;
	if (pmaxc) *pmaxv = maxc;
}




int main()
{
	printf("hello\n");
	
	ImgMap_t * Test = MakeIntMap(30,15);
	Test->values[Test->w*2+1] = 1;
	Test->values[Test->w*6+6] = 3;
	Test->values[Test->w*11+26] = 4;
	Test->values[Test->w*12+27] = 4;
	Test->values[Test->w*13+28] = 4;
	Test->values[Test->w*14+29] = 4;
	ShowImgMap(Test,1);

	BloomIntMap(Test);
	printf("Bloomed map:\n");
	ShowImgMap(Test,1);

	//BloomIntMap(Test);
	//BloomIntMap(Test);
	//printf("Bloomed map:\n");
	
	BlockFilterIntMap(Test, 3,4, NULL, NULL, NULL);
	printf("Block filtered map:\n");
	ShowImgMap(Test,1);
	
}