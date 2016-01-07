//----------------------------------------------------------------------------------------
// Code for using libjpeg to read an image into memory for imgcomp
// Based on code from:  stackoverflow.com/questions/5616216/need-help-in-reading-jpeg-file-using-libjpeg
//
// Imgcomp is licensed under GPL v2 (see README.txt)
//----------------------------------------------------------------------------------------
#include <stdio.h>
#include <stdio.h>
#include <malloc.h>
#include <stddef.h>
#include <setjmp.h>
#include "../libjpeg/jpeglib.h"
#include "../libjpeg/jerror.h"
#include <time.h>
#include "imgcomp.h"
#include "jhead.h"

//----------------------------------------------------------------------------------------
// for libjpeg - don't abort on corrupt jpeg data.
//----------------------------------------------------------------------------------------
struct my_error_mgr {
    struct jpeg_error_mgr pub; // "public" fields 

    jmp_buf setjmp_buffer;	// for return to caller
};
typedef struct my_error_mgr * my_error_ptr;

void my_error_exit (j_common_ptr cinfo)
{
    // cinfo->err really points to a my_error_mgr struct, so coerce pointer
    my_error_ptr myerr = (my_error_ptr) cinfo->err;

    // Always display the message.
    (*cinfo->err->output_message) (cinfo);

    // Return control to the setjmp point
    longjmp(myerr->setjmp_buffer, 1);
}


//----------------------------------------------------------------------------------------
// Use libjpeg to load an image into memory, optionally scale it.
//----------------------------------------------------------------------------------------
MemImage_t * LoadJPEG(char* FileName, int scale_denom, int discard_colors, int ParseExif)
{
    unsigned long data_size;    // length of the file
    struct jpeg_decompress_struct info; //for our jpeg info
    struct my_error_mgr jerr;
    MemImage_t *MemImage;
    int components;
    FILE* file = fopen(FileName, "rb");

    if(file == NULL) {
       fprintf(stderr, "Could not open file: \"%s\"!\n", FileName);
       return NULL;
    }

    if (ParseExif){
        // Get the exif header
        ReadExifPart(file);
    }

    info.err = jpeg_std_error(& jerr.pub);     
    jerr.pub.error_exit = my_error_exit; // Override library's default exit on error.


    if (setjmp(jerr.setjmp_buffer)) {
        // If we get here, the JPEG code has signaled an error.
        // We need to clean up the JPEG object, close the input file, and return.
        jpeg_destroy_decompress(&info);
        fclose(file);
        return NULL;
    }

    jpeg_create_decompress(& info);   // fills info structure

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
        jpeg_read_scanlines(&info, rowptr, 1);
    }
    //---------------------------------------------------

    jpeg_finish_decompress(&info);   //finish decompressing
    fclose(file);                    //close the file

    jpeg_destroy_decompress(&info);

    return MemImage;
}



//----------------------------------------------------------------------------------------
// Write an image to disk - for testing.  Not jpeg (ppm is a much simpler format)
//----------------------------------------------------------------------------------------
void WritePpmFile(char * FileName, MemImage_t *MemImage)
{
    FILE * outfile;
    outfile = fopen(FileName,"wb");
    if (outfile == NULL){
        printf("could not open outfile\n");
        return;
    }
    fprintf(outfile, "P%c\n%d %d\n%d\n", MemImage->components == 3 ? '6' : '5',  
            MemImage->width, MemImage->height, 255);
    fwrite(MemImage->pixels, 1, MemImage->width * MemImage->height*MemImage->components, outfile);
    fclose(outfile);
}
