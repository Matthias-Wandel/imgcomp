//----------------------------------------------------------------------------
// image comparator tool prototypes
//----------------------------------------------------------------------------

typedef struct {
    int width;
    int height;
    int components;
    unsigned char pixels[1];
}MemImage_t;

MemImage_t MemImage;

void WritePpmFile(char * FileName, MemImage_t *MemImage);
MemImage_t * LoadJPEG(char* FileName, int scale_denom, int discard_colors);
int ComparePix(MemImage_t * pic1, MemImage_t * pic2, char * DebugImgName);
