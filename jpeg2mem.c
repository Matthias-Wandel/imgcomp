
// from:  stackoverflow.com/questions/5616216/need-help-in-reading-jpeg-file-using-libjpeg
#include <stdio.h>
#include <malloc.h>
#include "libjpeg/jpeglib.h"
#include "libjpeg/jerror.h"
//================================

int LoadJPEG(char* FileName)
{
    unsigned long x, y;
    unsigned long data_size;     // length of the file
    int channels;               //  3 =>RGB   4 =>RGBA 
    unsigned char * rowptr[1];  // pointer to an array
    unsigned char * jdata;      // data for the image
    struct jpeg_decompress_struct info; //for our jpeg info
    struct jpeg_error_mgr err;          //the error handler

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

            info.out_color_space = JCS_GRAYSCALE;
            info.num_components = 3;
            info.do_fancy_upsampling = FALSE;
            info.scale_num = 1;
            info.scale_denom = 8;

    jpeg_start_decompress(&info);    // decompress the file

    //set width and height
    x = info.output_width;
    y = info.output_height;
    channels = info.num_components;
    printf("channels = %d\n",channels);

    data_size = x * y * channels;

    //--------------------------------------------
    // read scanlines one at a time & put bytes 
    //    in jdata[] array. Assumes an RGB image
    //--------------------------------------------
    jdata = (unsigned char *)malloc(data_size);
    while (info.output_scanline < info.output_height){ // loop
        // Enable jpeg_read_scanlines() to fill our jdata array
        rowptr[0] = (unsigned char *)jdata +  // secret to method
            3* info.output_width * info.output_scanline; 
printf("r");
        jpeg_read_scanlines(&info, rowptr, 1);
    }
    //---------------------------------------------------

    jpeg_finish_decompress(&info);   //finish decompressing

    fclose(file);                    //close the file

    // data_size now contains the data.

    {
        FILE * outfile;
        outfile = fopen("out.ppm","wb");
        if (outfile == NULL){
            printf("could not open outfile\n");
            return 0;
        }
        fprintf(outfile, "P6\n%ld %ld\n%d\n", info.output_width, info.output_height, 255);
        fwrite(jdata, 1, data_size, outfile);
        fclose(outfile);
    }

    free(jdata);

    return 1;
}
