//----------------------------------------------------------------------------------
// On the fly thumbnail maker
// Uses libjpeg to load image at less than full resolution,
// then write HTTP image header and image to stdout.
//---------------------------------------------------------------------------------- 
#include <stdio.h>
#include <stddef.h>
#include <errno.h>
#include <memory.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <jpeglib.h>
#include <jerror.h>
#include <setjmp.h>

typedef struct {
    int width;
    int height;
    int components;
    unsigned char pixels[1];
}MemImage_t;

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
MemImage_t * LoadJPEG(char* FileName, int scale_denom)
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

//----------------------------------------------------------------------------------
// Write jpeg file
//----------------------------------------------------------------------------------
void SaveJPEG(FILE * outfile, MemImage_t * MemImage)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr       jerr;
    int width;

    //Now, lets setup the libjpeg objects.

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, outfile);
 
    cinfo.image_width      = MemImage->width;
    cinfo.image_height     = MemImage->height;
    cinfo.input_components = MemImage->components;
    cinfo.in_color_space   = JCS_RGB;

    //Call the setup defualts helper function to give us a starting point.
    //Note, don’t call any of the helper functions before you call this,
    //they will no effect if you do. Then call other helper functions,
    //here I call the set quality. Then start the compression.

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality (&cinfo, 75, 1); // Set quality.
    jpeg_start_compress(&cinfo, 1);

    //Next we setup a pointer and start looping though lines in the image.
    //Notice that the row_pointer is set to the first byte of the row via
    //some pointer math/magic. Once the pointer is calculated, do a write_scanline.

    width = MemImage->width;
    while (cinfo.next_scanline < cinfo.image_height) {
        JSAMPROW row_pointer = (JSAMPROW) MemImage->pixels+width*cinfo.next_scanline*3;
        jpeg_write_scanlines(&cinfo, &row_pointer, 1);
    }

    // Finally, call finish 

    jpeg_finish_compress(&cinfo);
}


//----------------------------------------------------------------------------------
//  Cale brightness for really dark images
//----------------------------------------------------------------------------------
void ScaleBrightness(MemImage_t * MemImage)
{
    int row, a, b, c, width;
    int satpix;
    int medpix;
    int Histogram[256];

    memset(Histogram, 0, sizeof(Histogram));
    
    width = MemImage->width;
    for (row=0;row<MemImage->height;row+=2){
        unsigned char * RowPointer;
        RowPointer = MemImage->pixels+row*width*3;
        for (c=0;c<MemImage->width*3;c+=6){
            Histogram[RowPointer[c]] += 1;
            Histogram[RowPointer[c+1]] += 1;
            Histogram[RowPointer[c+2]] += 1;
        }
    }
    
    //for (a=0;a<256;a++) printf("%3d %d\n",a,Histogram[a]);
    
    // figure out what threshold value has no more than 0.4% of pixels above.
    satpix = (width * MemImage->height / 4) >> 8; // Pixesl near saturation
    medpix = (width * MemImage->height / 4) >> 2; // Don't make the image overall too bright.
    for (a=255;a>=0;a--){
        satpix -= Histogram[a];
        if (satpix <= 0) break;
    }
    for (b=255;b>=0;b--){
        medpix -= Histogram[a];
        if (medpix <= 0) break;
    }

    
    //printf("scale %d to 255\n",a);
    
    // If image is kind of dark, cale the brightness so that no more than 0.1% of the
    // pixels will saturate.
    if (a < 200){
        int Mult, Mult2;
        Mult = 256*240/a;
        Mult2 = 256*160/a;
        if (Mult2 < Mult) Mult = Mult2;
        if (Mult > 265*10) Mult = 256*10; // Don't boost by more than 10x.
        
        for (row=0;row<MemImage->height;row++){
            unsigned char * RowPointer;
            RowPointer = MemImage->pixels+row*width*3;
            for (c=0;c<MemImage->width*3;c+=3){
                int nv;
                nv = (RowPointer[c  ]*Mult)>>8;
                if (nv > 255) nv = 255;
                RowPointer[c  ] = nv;

                nv = (RowPointer[c+1]*Mult)>>8;
                if (nv > 255) nv = 255;
                RowPointer[c+1] = nv;

                nv = (RowPointer[c+2]*Mult)>>8;
                if (nv > 255) nv = 255;
                RowPointer[c+2] = nv;
            }
        }
    }
    
    
    
}



//----------------------------------------------------------------------------------
// Main
//----------------------------------------------------------------------------------
int main(int argc, char ** argv)
{
    int a,b;
	char * qenv;
	char FileName[100];
    int ScaleFactor = 8;
    int ScaleBrightnessOn = 1;

	printf("Content-Type: image/jpg\n\n"); // heder for image type.
	
	qenv = getenv("QUERY_STRING");	
	
	if (qenv == NULL){
		printf("No query string\n");
		exit(0);
	}
	for (a=b=0;a<100;a++){
        if (qenv[a] == '\0' || qenv[a] == '$') break;
		if (qenv[a] == '%' && qenv[a+1] == '2' && qenv[a+2] == '0'){
			FileName[b++] = ' ';
			a += 2;
		}else{
			FileName[b++] = qenv[a];
		}
	}
    FileName[b] = '\0';

    // Any characters past a '$' in the query string indicate image parameters
    for (;qenv[a];a++){
        switch(qenv[a]){
            case '1': ScaleFactor = 1; break;
            case '2': ScaleFactor = 2; break;
            case '4': ScaleFactor = 4; break;
            case '8': ScaleFactor = 8; break;
            case 'b': ScaleBrightnessOn = 0; break;
            case 'B': ScaleBrightnessOn = 1; break;
        }
    }

    {
        MemImage_t * Image;
        Image = LoadJPEG(FileName, ScaleFactor);
        if (ScaleBrightnessOn) ScaleBrightness(Image);
        SaveJPEG(stdout, Image);
    }
}

