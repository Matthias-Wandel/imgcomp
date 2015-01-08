//----------------------------------------------------------------------------------------
// Code for using libjpeg to read an image into memory.
// from:  stackoverflow.com/questions/5616216/need-help-in-reading-jpeg-file-using-libjpeg
//----------------------------------------------------------------------------------------
#include <stdio.h>
#include <stdio.h>
#include <malloc.h>
#include <stddef.h>
#include "libjpeg/jpeglib.h"
#include "libjpeg/jerror.h"

typedef struct {
    int width;
    int height;
    int components;
    unsigned char pixels[1];
}MemImage_t;

MemImage_t MemImage;


//----------------------------------------------------------------------------------------
// Write an image to disk - for testing.
//----------------------------------------------------------------------------------------
void WritePpmFile(char * FileName, MemImage_t *MemImage);
{
    FILE * outfile;
    outfile = fopen("out.ppm","wb");
    if (outfile == NULL){
        printf("could not open outfile\n");
        return;
    }
    fprintf(outfile, "P6\n%ld %ld\n%d\n", info.output_width, info.output_height, 255);
    fwrite(MemImage->pixels, 1, MemImage.width * MemImage.height*MemImage.components, outfile);
    fclose(outfile);
}



//----------------------------------------------------------------------------------------
// Use libjpeg to load an image into memory, optionally scale it.
//----------------------------------------------------------------------------------------
int LoadJPEG(char* FileName)
{
    unsigned long data_size;    // length of the file
    struct jpeg_decompress_struct info; //for our jpeg info
    struct jpeg_error_mgr err;          //the error handler
    MemImage_t *MemImage;

    FILE* file = fopen(FileName, "rb");  //open the file

    info.err = jpeg_std_error(& err);     
    jpeg_create_decompress(& info);   //fills info structure

    //if the jpeg file doesn't load
    if(!file) {
       fprintf(stderr, "Error reading JPEG file %s!", FileName);
       return 0;
    }

    jpeg_stdio_src(&info, file);    
    jpeg_read_header(&info, TRUE);   // read jpeg file header

//    info.out_color_space = JCS_GRAYSCALE;
    info.num_components = 3;
    info.do_fancy_upsampling = FALSE;
    info.scale_num = 1;
    info.scale_denom = 8;

    jpeg_start_decompress(&info);    // decompress the file

    data_size = info.output_width 
              * info.output_height * info.num_components;

    MemImage = malloc(data_size+offsetof(MemImage_t, pixels));
    if (!MemImage){
        fprintf(stderr, "Image malloc failed");
        return 0;
    }
    MemImage->width = info.output_width;
    MemImage->height = info.output_height;
    MemImage->components = info.num_components;

    //--------------------------------------------
    // read scanlines one at a time. Assumes an RGB image
    //--------------------------------------------
    while (info.output_scanline < info.output_height){ // loop
        unsigned char * rowptr[1];  // pointer to an array

        // Enable jpeg_read_scanlines() to fill our jdata array
        rowptr[0] = MemImage->pixels + 3 * info.output_width * info.output_scanline; 
printf("r");
        jpeg_read_scanlines(&info, rowptr, 1);
    }
    //---------------------------------------------------

    jpeg_finish_decompress(&info);   //finish decompressing

    fclose(file);                    //close the file


    WritePpmFile("out.ppm", MemImage);

    free(MemImage);

    return 1;
}
