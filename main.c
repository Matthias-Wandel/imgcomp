//-----------------------------------------------------------------------------------
// Simple tool for monitor continuously captured images for changes
// and saving changed images, as well as an image every N seconds for timelapses.
//-----------------------------------------------------------------------------------
#include <stdio.h>
#include <ctype.h>		// to declare isupper(), tolower() 
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
    #include "readdir.h"
    #define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
    #define strdup(a) _strdup(a) 
    extern void sleep(int);
    #define unlink(n) _unlink(n)
#else
    #include <dirent.h>
    #include <unistd.h>
#endif

#include "imgcomp.h"

// Configuration variables.
static const char * progname;  // program name for error messages
static char DoDirName[200];
static char SaveDir[200];
int FollowDir = 0;
static int ScaleDenom;

static char DiffMapFileName[200];
Regions_t Regions;


int Verbosity = 0;
static int Sensitivity;
static int Raspistill_restarted;
int TimelapseInterval;
#define MAX_RAPISTILL_CMD
char raspistill_cmd[200];


//-----------------------------------------------------------------------------------
// Indicate command line usage.
//-----------------------------------------------------------------------------------
void usage (void)// complain about bad command line 
{
    fprintf(stderr, "usage: %s [switches] ", progname);
    fprintf(stderr, "inputfile outputfile\n");

    fprintf(stderr, "Switches (names may be abbreviated):\n");
    fprintf(stderr, " -scale   N           Scale before detection by 1/N.  Default 1/4\n");
    fprintf(stderr, " -region  x1-x2,y1-y2 Specify region of interest\n");
    fprintf(stderr, " -exclude x1-x2,y1-y2 Exclude from area of interest\n");
    fprintf(stderr, " -diffmap <filename>  A file to use as diff map\n");
    fprintf(stderr, " -dodir   <srcdir>    Compare images in dir, in order\n");
    fprintf(stderr, " -followdir <srcdir>  Do dir and monitor for new images\n");
    fprintf(stderr, " -savedir <saveto>    Where to save images with changes\n");
    fprintf(stderr, " -sensitivity N       Set sensitivity\n");
    fprintf(stderr, " -tl N                Save image every N seconds regardless\n");

    fprintf(stderr, " -outfile name  Specify name for output file\n");
    fprintf(stderr, "  -verbose  or  -debug   Emit debug output\n");
    exit(-1);
}

//-----------------------------------------------------------------------------------
// Case-insensitive matching of possibly-abbreviated keyword switches.
// keyword is the constant keyword (must be lower case already),
// minchars is length of minimum legal abbreviation.
//-----------------------------------------------------------------------------------
static int keymatch (const char * arg, const char * keyword, int minchars)
{
    int ca, ck, nmatched = 0;

    while ((ca = *arg++) != '\0') {
        if ((ck = *keyword++) == '\0') return 0;  // arg longer than keyword, no good */
        if (isupper(ca))  ca = tolower(ca);	 // force arg to lcase (assume ck is already)
        if (ca != ck)  return 0;             // no good 
        nmatched++;			                 // count matched characters 
    }
    // reached end of argument; fail if it's too short for unique abbrev 
    if (nmatched < minchars) return 0;
    return 1;
}

//-----------------------------------------------------------------------------------
// Parse a region parameter.
//-----------------------------------------------------------------------------------
static int ParseRegion(Region_t * reg, const char * value)
{
    char * t;
    t = strstr(value, ",");
    if (t == NULL || t != value){
        // No comma, or comma not in first position.  Parse X parameters.
        if (sscanf(value, "%d-%d", &reg->x1, &reg->x2) != 2){
            if (sscanf(value, "%d+%d", &reg->x1, &reg->x2) != 2) return 0;
            reg->x2 += reg->x1;
        }
    }
    if (t != NULL){
        // Y parameters come after the comma.
        if (sscanf(t+1, "%d-%d", &reg->y1, &reg->y2) != 2){
            if (sscanf(t+1, "%d+%d", &reg->y1, &reg->y2) != 2) return 0;
            reg->y2 += reg->y1;
        }
    }
    if (reg->y2-reg->y1 < 16 || reg->x2-reg->x1 < 16){
        fprintf(stderr,"Detect region is too small\n");
        exit(-1);
    }
    printf("Region is x:%d-%d, y:%d-%d\n",reg->x1, reg->x2, reg->y1, reg->y2);
    return 1;
}


//-----------------------------------------------------------------------------------
// Parse command line switches
// Returns argv[] index of first file-name argument (== argc if none).
//-----------------------------------------------------------------------------------
static int parse_parameter (const char * tag, const char * value)
{
    if (keymatch(tag, "debug", 1) || keymatch(tag, "verbose", 1)) {
        // Enable debug printouts.  Specify more than once for more detail.
        Verbosity += 1;
        return 1;

    } else if (keymatch(tag, "scale", 2)) {
        // Scale the output image by a fraction M/N.
        if (!value) goto need_val;
        if (sscanf(value, "%d", &ScaleDenom) != 1)
           usage();
    } else if (keymatch(tag, "sensitivity", 2)) {
        // Scale the output image by a fraction M/N.
        if (!value) goto need_val;
        if (sscanf(value, "%d", &Sensitivity) != 1)
           usage();
    } else if (keymatch(tag, "timelapse", 1)) {
        // Scale the output image by a fraction M/N.
        if (!value) goto need_val;
        if (sscanf(value, "%d", (int *)&TimelapseInterval) != 1)
           usage();
        if (TimelapseInterval < 1){
            fprintf(stderr,"timelapse interval must be at least 1 second\n");
            exit(-1);
        }
    } else if (keymatch(tag, "aquire_cmd", 4)) {
        // Set output file name.
        if (!value) goto need_val;
        strncpy(raspistill_cmd, value, sizeof(raspistill_cmd)-1);
    } else if (keymatch(tag, "savedir", 4)) {
        // Set output file name.
        if (!value) goto need_val;
        strncpy(SaveDir,value, sizeof(SaveDir)-1);

    } else if (keymatch(tag, "dodir", 2)) {
        // Scale the output image by a fraction M/N. */
        if (!value) goto need_val;
        strncpy(DoDirName,value, sizeof(DoDirName)-1);
    } else if (keymatch(tag, "region", 2)) {
        if (!value) goto need_val;
        if (!ParseRegion(&Regions.DetectReg, value)) goto bad_value;
        printf("Region is x:%d-%d, y:%d-%d\n",
            Regions.DetectReg.x1, Regions.DetectReg.x2, 
            Regions.DetectReg.y1, Regions.DetectReg.y2);
    } else if (keymatch(tag, "exclude", 2)) {
        if (!value) goto need_val;

        if (Regions.NumExcludeReg >= MAX_EXCLUDE_REGIONS){
            fprintf(stderr, "too many exclude regions");
            exit(-1);
        }else{
            Region_t NewEx;
            if (!ParseRegion(&NewEx, value)) goto bad_value;
            printf("Exclude region x:%d-%d, y:%d-%d\n",NewEx.x1, NewEx.x2, NewEx.y1, NewEx.y2);
            Regions.ExcludeReg[Regions.NumExcludeReg++] = NewEx;
        }
    } else if (keymatch(tag, "diffmap", 2)) {
        if (!value) goto need_val;
        strncpy(DiffMapFileName,value, sizeof(DiffMapFileName)-1);


    } else if (keymatch(tag, "followdir", 2)) {
        if (!value){
            need_val:
            fprintf(stderr, "Parameter '%s' needs to be followed by a vaue\n",tag);
            usage();
        }
        strncpy(DoDirName,value, sizeof(DoDirName)-1);
        FollowDir = 1;
    } else {
        fprintf(stderr,"argument %s not understood\n\n",tag);
        usage();	   // bogus switch
        bad_value:
        fprintf(stderr, "Value of %s=%s\n not understood\n",tag,value);
        usage();

    }
    return 2;
}


//-----------------------------------------------------------------------------------
// Parse command line switches
// Returns argv[] index of first file-name argument (== argc if none).
//-----------------------------------------------------------------------------------
static int parse_switches (int argc, char **argv, int last_file_arg_seen, int for_real)
{
    int argn;
    char * arg;
    char * value;

    // Scan command line options, adjust parameters
    for (argn = 1; argn < argc;) {
        //printf("argn = %d\n",argn);
        arg = argv[argn];
        if (*arg != '-') {
            return argn;
        }
        value = NULL;
        if (argn+1 < argc) value = argv[argn+1];

        argn += parse_parameter(arg+1, value);
    }
    return argc;
}

//-----------------------------------------------------------------------------------
// Too many parameters for imgcomp running.  Just read them from a configuration file.
//-----------------------------------------------------------------------------------
static void read_config_file()
{
    FILE * file;
    char ConfLine[201];

    file = fopen("imgcomp.conf", "r");
    if (file == NULL){
        fprintf(stderr, "No configuration file imgcomp.conf\n");
        return;
    }
    for(;;){
        char *s, *value, *t;
        int len;
        s = fgets(ConfLine, sizeof(ConfLine)-1, file);
        ConfLine[sizeof(ConfLine)-1] = '\0';
        if (s == NULL) break;

        // Remove leading spaces
        while (*s == ' ' || *s == '\t') s++;

        // Check line length not exceeded.
        len = strlen(s);
        if(len >= sizeof(ConfLine)-1){
            fprintf(stderr, "Configuration line too long:\n%s",s);
            exit(-1);
        }

        // Remove trailing spaces and linefeed.
        t = s+len-1;
        while (*t <= ' ' && t > s) *t-- = '\0';

        if (*s == '#') continue; // comment.
        if (*s == '\r' || *s == '\n') continue; // Blank line.

        value = strstr(s, "=");
        if (value != NULL){
            t = value;

            // Remove value leading spaces
            value += 1;
            while (*value == ' ' || *value == '\t') value++;

            // remove tag trailing spaces.
            *t = '\0';
            t--;
            while (*t == ' ' || *t == '\t'){
                *t = '\0';
                t--;
            }
        }
        // Now finally have the tag extracted.
        printf("'%s' = '%s'\n",s, value);

        parse_parameter(s,value);
    }
}



static MemImage_t *LastPic;
static char LastPicName[500];
static int LastDiffMag;
static char * LastPicSaveName = NULL;

//-----------------------------------------------------------------------------------
// Process a whole directory of files.
//-----------------------------------------------------------------------------------
static int DoDirectoryFunc(char * Directory, char * KeepPixDir, int Delete)
{
    char ** FileNames;
    int NumEntries;
    int a;
    int ReadExif;
    int NumProcessed;
    int SawMotion;

    static MemImage_t *CurrentPic;
    static char * CurrentPicName;
    static int PixSinceDiff;

    SawMotion = 0;

    FileNames = GetSortedDir(Directory, &NumEntries);
    if (FileNames == NULL) return 0;
    if (NumEntries == 0) return 0;

    a=0;
    if (strcmp(LastPicName, FileNames[0]) == 0){
        // Don't redo the last picture.
        a += 1;
    }

    ReadExif = 1;
    NumProcessed = 0;
    for (;a<NumEntries;a++){
        int diff = 0;
        //printf("sorted dir: %s\n",FileNames[a]);
        CurrentPicName = FileNames[a];
        CurrentPic = LoadJPEG(CatPath(Directory, CurrentPicName), ScaleDenom, 0, ReadExif);
        ReadExif = 0; // Only read exif for first image.
        if (CurrentPic == NULL){
            fprintf(stderr, "Failed to load %s\n",CatPath(Directory, CurrentPicName));
            if (Delete){
                // Raspberry pi timelapse mode may at times dump a corrupt
                // picture at the end of timelapse mode.  Just delete and go on.
                unlink(CatPath(Directory, CurrentPicName));
            }
            continue;
        }
        NumProcessed += 1;
        if (LastPic != NULL && CurrentPic != NULL){
            TriggerInfo_t Trig;
            int diff;
            
            Trig = ComparePix(LastPic, CurrentPic, NULL);
            diff = Trig.DiffLevel;

            if (diff >= Sensitivity && PixSinceDiff > 5 && Raspistill_restarted){
                printf("Ignoring diff %d caused by raspistill restart\n",diff);
                diff = 0;
            }
            
            printf("%s - %s:",LastPicName, CurrentPicName);            
            printf(" %3d at (%4d,%3d) ", Trig.DiffLevel, Trig.x*ScaleDenom, Trig.y*ScaleDenom);
            
            if (diff >= Sensitivity){
                if (!LastPicSaveName){
                    // Save the pre-motion pic if it wasn't already saved.
                    BackupPicture(Directory, LastPicName, KeepPixDir, LastDiffMag, 1);
                }
                LastPicSaveName = BackupPicture(Directory, CurrentPicName, KeepPixDir, diff, 1);
                if (LastPicSaveName) printf("  Motion pic %s",LastPicSaveName);
                PixSinceDiff = 0;
                SawMotion = 1;
            }else{
                LastPicSaveName = BackupPicture(Directory, CurrentPicName, KeepPixDir, diff, 0);
                if (LastPicSaveName) printf("  Timelapse pic %s",LastPicSaveName);
                PixSinceDiff ++;
            }
            if (!LastPicSaveName) printf("                ");
            printf("\n");
            Raspistill_restarted = 0;
        }

        if (LastPic != NULL) free(LastPic);
        if (Delete){
            unlink(CatPath(Directory, LastPicName));
        }

        if (CurrentPic != NULL){
            LastPic = CurrentPic;
            strcpy(LastPicName, CurrentPicName);
            LastDiffMag = diff;
            CurrentPic = NULL;
        }
    }

    FreeDir(FileNames, NumEntries); // Free up the whole directory structure.
    FileNames = NULL;

    if (SawMotion){
        run_blink_program();
    }

    return NumProcessed;
}


//-----------------------------------------------------------------------------------
// Process a whole directory of files.
//-----------------------------------------------------------------------------------
int DoDirectory(char * Directory, char * KeepPixDir)
{
    int a,b;
    Raspistill_restarted = 0;

    for (;;){
        a = DoDirectoryFunc(Directory, KeepPixDir, FollowDir);
        if (FollowDir){
            b = manage_raspistill(a);
            if (b) Raspistill_restarted = 1;
            sleep(1);
        }else{
            break;
        }
    }
    if (LastPic != NULL) free(LastPic);
    return a;
}

static void ScaleRegion (Region_t * Reg, int Denom)
{
    Reg->x1 /= Denom;
    Reg->x2 /= Denom;
    Reg->y1 /= Denom;
    Reg->y2 /= Denom;
}

//-----------------------------------------------------------------------------------
// The main program.
//-----------------------------------------------------------------------------------
int main(int argc, char **argv)
{
    int file_index, a;
    progname = argv[0];

    // Reset to default parameters.
    ScaleDenom = 4;
    DoDirName[0] = '\0';
    Sensitivity = 10;
    Regions.DetectReg.x1 = 0;
    Regions.DetectReg.x2 = 1000000;
    Regions.DetectReg.y1 = 0;
    Regions.DetectReg.y2 = 1000000;
    Regions.NumExcludeReg = 0;
    TimelapseInterval = 0;

    // First read the configuration file.
    read_config_file();

    // Get command line arguments (which may override configuration file)
    file_index = parse_switches(argc, argv, 0, 0);

    // Adjust region of interest to scale.
    ScaleRegion(&Regions.DetectReg, ScaleDenom);
    for (a=0;a<Regions.NumExcludeReg;a++){
        ScaleRegion(&Regions.ExcludeReg[a], ScaleDenom);
    }

    if (DoDirName[0]) printf("Source directory = %s, follow=%d\n",DoDirName, FollowDir); 
    if (SaveDir[0]) printf("Save to dir %s\n",SaveDir);
    if (TimelapseInterval) printf("Timelapse interval %d seconds\n",TimelapseInterval);

    if (DiffMapFileName[0]){
        MemImage_t *MapPic;
        printf("Diffmap file: %s\n",DiffMapFileName);
        if (Regions.DetectReg.x1 || Regions.DetectReg.y1
                || (Regions.DetectReg.x2 <  100000) || (Regions.DetectReg.y2 < 100000)){
            fprintf(stderr, "Specify diff map or detect regions, but not both\n");
            exit(-1);
        }

        if (Regions.NumExcludeReg){
            fprintf(stderr, "Specify diff map or exclude regions, but not both\n");
            exit(-1);
        }

        MapPic  = LoadJPEG(DiffMapFileName, ScaleDenom, 0, 0);
        ProcessDiffMap(MapPic);

        free(MapPic);
    }

  
    if (DoDirName[0] && file_index == argc){
        // if dodir is specified in config file, but files are specified
        // on the command line, do the files instead.
        DoDirectory(DoDirName, SaveDir);
    }

    if (argc-file_index == 2){
        MemImage_t *pic1, *pic2;
        
        printf("load %s\n",argv[file_index]);
        pic1 = LoadJPEG(argv[file_index], ScaleDenom, 0, 0);
        printf("\nload %s\n",argv[file_index+1]);
        pic2 = LoadJPEG(argv[file_index+1], ScaleDenom, 0, 0);
        if (pic1 && pic2){
            Verbosity = 2;
            ComparePix(pic1, pic2, "diff.ppm");
        }
        free(pic1);
        free(pic2);
    }else{
        int a;
        MemImage_t * pic;

        for (a=file_index;a<argc;a++){

            printf("input file %s\n",argv[a]);
            // Load file into memory.

            pic = LoadJPEG(argv[a], 4, 0, 0);
            if (pic) WritePpmFile("out.ppm",pic);
        }
    }
    return 0;
}

// Features to consider adding:
//--------------------------------------------------------
// Ignore regions, focus regions (Threshold an image for this)
// Polling same file mode (for use with  uvccapture)
// Delete old saved images if too many saved.
// Dynamic thresholding?

