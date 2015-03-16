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

#define MAX_EXCLUDE_REGIONS 5
typedef struct {
    Region_t DetectReg;
    Region_t ExcludeReg[MAX_EXCLUDE_REGIONS];
    int NumExcludeReg;
}Regions_t;


typedef struct {
    int DiffLevel;
    int x, y;
}TriggerInfo_t;

MemImage_t MemImage;
extern int NewestAverageBright;
extern int TimelapseInterval;
extern int FollowDir;
extern int Verbosity;

extern Regions_t Regions;

// compare.c functions
TriggerInfo_t ComparePix(MemImage_t * pic1, MemImage_t * pic2, char * DebugImgName);
void ProcessDiffMap(MemImage_t * MapPic);

// jpeg2mem.c functions
MemImage_t * LoadJPEG(char* FileName, int scale_denom, int discard_colors, int ParseExif);
void WritePpmFile(char * FileName, MemImage_t *MemImage);

// start_raspistill functions
int manage_raspistill(int HaveNewImages);
extern char raspistill_cmd[200];

// util.c functions
char * CatPath(char *Dir, char * FileName);
char ** GetSortedDir(char * Directory, int * NumFiles);
void FreeDir(char ** FileNames, int NumEntries);
char * BackupPicture(char * Directory, char * Name, char * KeepPixDir, int Threshold, int MotionTriggered);
int CopyFile(char * src, char * dest);