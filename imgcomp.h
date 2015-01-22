//----------------------------------------------------------------------------
// image comparator tool prototypes
//----------------------------------------------------------------------------

typedef struct {
    int width;
    int height;
    int components;
    unsigned char pixels[1];
}MemImage_t;

typedef struct {
    int x1, x2;
    int y1, y2;
}Region_t;

MemImage_t MemImage;

void WritePpmFile(char * FileName, MemImage_t *MemImage);
MemImage_t * LoadJPEG(char* FileName, int scale_denom, int discard_colors, int ParseExif);
int ComparePix(MemImage_t * pic1, MemImage_t * pic2, Region_t reg, char * DebugImgName, int Verbosity);
int CopyFile(char * src, char * dest);

int launch_raspistill(void);
void manage_raspistill(int HaveNewImages);