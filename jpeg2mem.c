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
#include "imgcomp.h"

//----------------------------------------------------------------------------------------
// Write an image to disk - for testing.
//----------------------------------------------------------------------------------------
void WritePpmFile(char * FileName, MemImage_t *MemImage)
{
    FILE * outfile;
    outfile = fopen("out.ppm","wb");
    if (outfile == NULL){
        printf("could not open outfile\n");
        return;
    }
    fprintf(outfile, "P%c\n%d %d\n%d\n", MemImage->components == 3 ? '6' : '5',  
            MemImage->width, MemImage->height, 255);
    fwrite(MemImage->pixels, 1, MemImage->width * MemImage->height*MemImage->components, outfile);
    fclose(outfile);
}



//----------------------------------------------------------------------------------------
// Use libjpeg to load an image into memory, optionally scale it.
//----------------------------------------------------------------------------------------
MemImage_t * LoadJPEG(char* FileName, int scale_denom, int discard_colors)
{
    unsigned long data_size;    // length of the file
    struct jpeg_decompress_struct info; //for our jpeg info
    struct jpeg_error_mgr err;          //the error handler
    MemImage_t *MemImage;
    int components;

    FILE* file = fopen(FileName, "rb");  //open the file

    info.err = jpeg_std_error(& err);     
    jpeg_create_decompress(& info);   //fills info structure

    //if the jpeg file doesn't load
    if(!file) {
       fprintf(stderr, "Error reading JPEG file %s!", FileName);
       return NULL;
    }

    jpeg_stdio_src(&info, file);    
    jpeg_read_header(&info, TRUE);   // read jpeg file header

    if (discard_colors) info.out_color_space = JCS_GRAYSCALE;

    info.scale_num = 1;
    info.scale_denom = scale_denom;

    info.num_components = 3;
    info.do_fancy_upsampling = FALSE;

    jpeg_start_decompress(&info);    // decompress the file

    components = info.out_color_space == JCS_GRAYSCALE ? 1 : 3;

    data_size = info.output_width 
              * info.output_height * components;

    

    MemImage = malloc(data_size+offsetof(MemImage_t, pixels));
    if (!MemImage){
        fprintf(stderr, "Image malloc failed");
        return 0;
    }
    MemImage->width = info.output_width;
    MemImage->height = info.output_height;
    MemImage->components = components;



    //--------------------------------------------
    // read scanlines one at a time. Assumes an RGB image
    //--------------------------------------------
    while (info.output_scanline < info.output_height){ // loop
        unsigned char * rowptr[1];  // pointer to an array

        // Enable jpeg_read_scanlines() to fill our jdata array
        rowptr[0] = MemImage->pixels + 
            components * info.output_width * info.output_scanline; 
printf("r");
        jpeg_read_scanlines(&info, rowptr, 1);
    }
    //---------------------------------------------------

    jpeg_finish_decompress(&info);   //finish decompressing

    fclose(file);                    //close the file
    return MemImage;
}
