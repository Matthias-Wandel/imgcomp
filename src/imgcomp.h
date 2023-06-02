//----------------------------------------------------------------------------
// imgcomp structures and function prototypes
// Matthias Wandel 2015-2020
//
// Imgcomp is licensed under GPL v2 (see README.txt)
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
	int Motion;
}TriggerInfo_t;

typedef struct {
    int w, h;
    int values[0];
}ImgMap_t;

typedef struct {
    int ISOmin, ISOmax; // Limits of ISO values to pass to raspistill
    float Tmin, Tmax;   // Limits of exposure time (in seconds) to pass to raspistill

    int SatVal;         // Saturated pixel value (some cameras saturate before 255)
    int ISOoverExTime;  // Target ISO/exposure time.  Larger values prioritize
                        // fast shutter speed at expenso of grainy photos.
}exconfig_t;

extern exconfig_t ex;


extern MemImage_t MemImage;
extern int NewestAverageBright;
extern int MsPerCycle; // HOw often to check for new images.
extern int Verbosity;
extern char LogToFile[200];
extern char MoveLogNames[200];
extern char TempDirName[200];
extern FILE * Log;

extern int BrightnessChangeRestart;

extern char SaveDir[200];
extern char SaveNames[200];
extern char CopyJpgCmd[200];

extern Regions_t Regions;
extern ImgMap_t * WeightMap;

extern time_t LastPic_mtime;

// Vidoe segment mode:
extern int VidMode; // Video mode flag
extern char VidDecomposeCmd[200];

// exposure.c functions
char * GetRaspistillExpParms();
int CalcExposureAdjust(MemImage_t * pic);


// compare_util.c functions
void FillWeightMap(int width, int height);
void ProcessDiffMap(MemImage_t * MapPic);
double AverageBright(MemImage_t * pic, Region_t Region, ImgMap_t* WeightMap);

ImgMap_t * MakeImgMap(int w,int h);
void ShowImgMap(ImgMap_t * map, int divisor);
void BloomImgMap(ImgMap_t * src, ImgMap_t * dst);
int BlockFilterImgMap(const ImgMap_t * src, int fw, int fh, int * pmaxc, int * pmaxr);

// compare.c function
TriggerInfo_t ComparePix(MemImage_t * pic1, MemImage_t * pic2, int UpdateFatigue, int SkipFatigue, char * DebugImgName, TriggerInfo_t * no_fatigue_motion);


// jpeg2mem.c functions
MemImage_t * LoadJPEG(char* FileName, int scale_denom, int discard_colors, int ParseExif);
void WritePpmFile(char * FileName, MemImage_t *MemImage);

// start_camera_prog functions
int relaunch_camera_prog(void);
int manage_camera_prog(int HaveNewImages);
void DoMotionRun(int SawMotion);
extern char camera_prog_cmd[200];
extern char lighton_run[200];
extern char lightoff_run[200];
extern int lightoff_min;
extern int lightoff_max;

// util.c functions
char * CatPath(char *Dir, char * FileName);
int EnsurePathExists(const char * FileName, int filepath);

typedef struct {
    unsigned int FileSize;
    time_t MTime;
    time_t ATime;
    char FileName[50];
}DirEntry_t;

DirEntry_t * GetSortedDir(char * Directory, int * NumFiles);
void FreeDir(DirEntry_t * FileNames, int NumEntries);
char * BackupImageFile(char * Name, int DiffMag, int DoNotCopy);
void LogFileMaintain(int ForceLotSave);


// send_udp.c functions
void SendUDP(int x, int y, int level, int motion);
int InitUDP(char * HostName);