//-----------------------------------------------------------------------------------
// Simple tool for monitor continuously captured images for changes
// and saving changed images, as well as an image every N seconds for timelapses.
// Matthias Wandel 2015
//
// Imgcomp is licensed under GPL v2 (see README.txt)
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
char SaveDir[200];
char SaveNames[200];
int FollowDir = 0;
static int ScaleDenom;
int SpuriousReject = 0;

static char DiffMapFileName[200];
Regions_t Regions;


int Verbosity = 0;
static int Sensitivity;
static int Raspistill_restarted;
int TimelapseInterval;
char raspistill_cmd[200];
char blink_cmd[200];


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
    fprintf(stderr, " -savenames <scheme>  Output naming scheme.  Uses strftime\n"
                    "                      to format the output name.  May include\n"
                    "                      '/' characters for directories.");
    fprintf(stderr, " -sensitivity N       Set sensitivity.  Lower=more sensitive\n");
    fprintf(stderr, " -blink_cmd <command> Run this command when motion detected\n"
                    "                      (used to blink the camera LED)\n");
    fprintf(stderr, " -tl N                Save image every N seconds regardless\n");
    fprintf(stderr, " -spurious            Ignore any change that returns to\n"
                    "                      previous image in the next frame\n");
    fprintf(stderr, " -verbose or -debug   Emit more verbose output\n");
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
    printf("    Region is x:%d-%d, y:%d-%d\n",reg->x1, reg->x2, reg->y1, reg->y2);
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
	}
	if (!value){
        fprintf(stderr, "Parameter '%s' needs to be followed by a vaue\n",tag);
        return -1;
	}

    if (keymatch(tag, "spurious", 4)) {
        SpuriousReject = value[0]-'0';
		if ((SpuriousReject != 0 && SpuriousReject != 1) || value[1] != 0){
			fprintf(stderr, "Spurious value can only be 0 or 1\n");
		}
    } else if (keymatch(tag, "scale", 2)) {
        // Scale the output image by a fraction 1/N.
        if (sscanf(value, "%d", &ScaleDenom) != 1) return -1;
    } else if (keymatch(tag, "sensitivity", 2)) {
        // Sensitivity level (lower = more senstitive) 
        if (sscanf(value, "%d", &Sensitivity) != 1) return -1;
    } else if (keymatch(tag, "timelapse", 4)) {
        // Scale the output image by a fraction M/N.
        if (sscanf(value, "%d", (int *)&TimelapseInterval) != 1) return -1;
        if (TimelapseInterval < 1){
            fprintf(stderr,"timelapse interval must be at least 1 second\n");
            return -1;
        }
    } else if (keymatch(tag, "aquire_cmd", 4)) {
        // Set output file name.
        strncpy(raspistill_cmd, value, sizeof(raspistill_cmd)-1);
    } else if (keymatch(tag, "blink_cmd", 5)) {
        // Set output file name.
        strncpy(blink_cmd, value, sizeof(blink_cmd)-1);
    } else if (keymatch(tag, "savedir", 4)) {
        // Set output file name.
        strncpy(SaveDir,value, sizeof(SaveDir)-1);
    } else if (keymatch(tag, "savenames", 5)) {
        // Set output file name.
        strncpy(SaveNames,value, sizeof(SaveNames)-1);
        {
            int a,b;
            for (a=0;SaveNames[a];a++){
                if (SaveNames[a] == '%'){
                    for (b=0;b<10;b++) if (SaveNames[a+1] == "dHjmMSUwyY"[b]) break;
                    if (b >=10){
                        fprintf(stderr, "savenames '%%' may only be followed by one of d,H,j,m,M,S,U,w,y, or Y\n");
                        exit(-1);
                    }
                }
            }
        }

    } else if (keymatch(tag, "region", 3)) {
        if (!ParseRegion(&Regions.DetectReg, value)) goto bad_value;
    } else if (keymatch(tag, "exclude", 4)) {
        if (Regions.NumExcludeReg >= MAX_EXCLUDE_REGIONS){
            fprintf(stderr, "too many exclude regions");
            exit(-1);
        }else{
            Region_t NewEx;
            if (!ParseRegion(&NewEx, value)) goto bad_value;
            printf("Exclude region x:%d-%d, y:%d-%d\n",NewEx.x1, NewEx.x2, NewEx.y1, NewEx.y2);
            Regions.ExcludeReg[Regions.NumExcludeReg++] = NewEx;
        }
    } else if (keymatch(tag, "diffmap", 5)) {
        strncpy(DiffMapFileName,value, sizeof(DiffMapFileName)-1);

    } else if (keymatch(tag, "dodir", 5)) {
        // Scale the output image by a fraction M/N. */
        strncpy(DoDirName,value, sizeof(DoDirName)-1);
		FollowDir = 0;
		
    } else if (keymatch(tag, "followdir", 6)) {
        strncpy(DoDirName,value, sizeof(DoDirName)-1);
        FollowDir = 1;
    } else {
        fprintf(stderr,"argument %s not understood\n",tag);
        return -1;	   // bogus switch
        bad_value:
        fprintf(stderr, "Value of %s=%s\n not understood\n",tag,value);
        return -1;

    }
    return 2;
}


//-----------------------------------------------------------------------------------
// Parse command line switches
// Returns argv[] index of first file-name argument (== argc if none).
//-----------------------------------------------------------------------------------
static int parse_switches (int argc, char **argv, int last_file_arg_seen)
{
    int argn,a;
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

        a = parse_parameter(arg+1, value);
        if (a < 0) usage();
        argn += a;
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
    int linenum = 0;

    file = fopen("imgcomp.conf", "r");
    if (file == NULL){
        fprintf(stderr, "No configuration file imgcomp.conf\n");
        return;
    }
    for(;;){
        char *s, *value, *t;
        int len, a;
        s = fgets(ConfLine, sizeof(ConfLine)-1, file);
        ConfLine[sizeof(ConfLine)-1] = '\0';
        if (s == NULL) break;
        linenum += 1;

        // Remove leading spaces
        while (*s == ' ' || *s == '\t') s++;

        // Check line length not exceeded.
        len = strlen(s);
        if(len >= sizeof(ConfLine)-1){
            fprintf(stderr, "Configuration line too long:\n%s",s);
            goto no_good;
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
        
        {
            static int msg_shown = 0;
            if (!msg_shown) printf("Configuration parameters:\n");
            msg_shown = 1;
        }

        // Now finally have the tag extracted.
        printf("    '%s' = '%s'\n",s, value);

        a = parse_parameter(s,value);
        if (a < 0){
no_good:            
            fprintf(stderr, "Error on line %d of imgcomp.conf",linenum);
            exit(-1);
        }

    }
}

typedef struct {
    MemImage_t *Image;
    char Name[500];
    int nind; // Name part index.
    unsigned  mtime;
    int DiffMag;
    //int Saved;
    int IsTimelapse;
    int IsMotion;
}LastPic_t;

static LastPic_t LastPics[3];
static time_t NextTimelapsePix;

//-----------------------------------------------------------------------------------
// Figure out which images should be saved.
//-----------------------------------------------------------------------------------
static int ProcessImage(LastPic_t * New)
{
    static int PixSinceDiff;

    LastPics[2] = LastPics[1];
    LastPics[1] = LastPics[0];

    LastPics[0] = *New;
    LastPics[0].IsMotion = LastPics[0].IsTimelapse = 0;

    if (LastPics[1].Image != NULL){
        TriggerInfo_t Trig;
        // Handle timelapsing.
        if (TimelapseInterval >= 1){
            if (LastPics[0].mtime >= NextTimelapsePix){
                LastPics[0].IsTimelapse = 1;
            }

            // Figure out when the next timelapse interval should be.
            NextTimelapsePix = LastPics[0].mtime+TimelapseInterval;
            NextTimelapsePix -= (NextTimelapsePix % TimelapseInterval);
//printf("Now  %d\nNext %d\n", LastPics[0].mtime, NextTimelapsePix);
        }

        // compare with previosu picture.
        Trig.DiffLevel = Trig.x = Trig.y = 0;

        if (LastPics[2].Image){
            Trig = ComparePix(LastPics[0].Image, LastPics[1].Image, NULL);
        }

        if (Trig.DiffLevel >= Sensitivity && PixSinceDiff > 5 && Raspistill_restarted){
            printf("Ignoring diff caused by raspistill restart\n");
            Trig.DiffLevel = 0;
        }
        LastPics[0].DiffMag = Trig.DiffLevel;

        printf("%s - %s:",LastPics[0].Name+LastPics[0].nind, LastPics[1].Name+LastPics[0].nind);
        printf(" %3d at (%4d,%3d) ", Trig.DiffLevel, Trig.x*ScaleDenom, Trig.y*ScaleDenom);

        if (LastPics[0].DiffMag > Sensitivity){
            LastPics[0].IsMotion = 1;
        }

        if (SpuriousReject && LastPics[2].Image && 
            LastPics[0].IsMotion && LastPics[1].IsMotion
            && LastPics[2].DiffMag < (Sensitivity>>1)){
            // Compare to picture before last picture.
            Trig = ComparePix(LastPics[0].Image, LastPics[2].Image, NULL);

            //printf("Diff with pix before last: %d\n",Trig.DiffLevel);
            if (Trig.DiffLevel < Sensitivity){
                // An event that was just one frame.  We assume this was something
                // spurious, like an insect or a rain drop
                printf("(spurious %d, ignore) ", Trig.DiffLevel);
                LastPics[0].IsMotion = 0;
                LastPics[1].IsMotion = 0;
            }
        }
        if (LastPics[0].IsMotion) printf("(motion) ");
        if (LastPics[0].IsTimelapse) printf("(time) ");
        if (!LastPics[0].IsMotion) printf("       "); // Overwrite rest of old line.

        if (LastPics[2].IsMotion || LastPics[2].IsTimelapse || LastPics[1].IsMotion){
            // If it's motion, pre-motion, or timelapse, save it.
            if (SaveDir[0]){
                BackupPicture(LastPics[2].Name, LastPics[2].mtime, LastPics[2].DiffMag);
            }
        }

        printf("\n");
        Raspistill_restarted = 0;
    }

    // Third picture now falls out of the window.  Free it and delete it.
    if (LastPics[2].Image != NULL) free(LastPics[2].Image);
    if (FollowDir){
        //printf("Delete %s\n",LastPics[2].Name);
        unlink(LastPics[2].Name);
    }

    return LastPics[0].IsMotion;
}

//-----------------------------------------------------------------------------------
// Process a whole directory of files.
//-----------------------------------------------------------------------------------
static int DoDirectoryFunc(char * Directory)
{
    char ** FileNames;
    int NumEntries;
    int a;
    int ReadExif;
    int NumProcessed;
    int SawMotion;

    SawMotion = 0;

    FileNames = GetSortedDir(Directory, &NumEntries);
    if (FileNames == NULL) return 0;
    if (NumEntries == 0) return 0;

    ReadExif = 1;
    NumProcessed = 0;
    for (a=0;a<NumEntries;a++){
        LastPic_t NewPic;
        struct stat statbuf;
        strcpy(NewPic.Name, CatPath(Directory, FileNames[a]));
        NewPic.nind = strlen(Directory)+1;

        if (strcmp(LastPics[0].Name, NewPic.Name) == 0
           || strcmp(LastPics[1].Name, NewPic.Name) == 0){
            continue; // Don't redo old pictures that we have looked at, but
                      // not yet deleted because we may still need them.
        }

        //printf("sorted dir: %s\n",FileNames[a]);
        NewPic.Image = LoadJPEG(NewPic.Name, ScaleDenom, 0, ReadExif);
        if (NewPic.Image == NULL){
            fprintf(stderr, "Failed to load %s\n",NewPic.Name);
            if (FollowDir){
                // Raspberry pi timelapse mode may at times dump a corrupt
                // picture at the end of timelapse mode.  Just delete and go on.
                unlink(NewPic.Name);
            }
            continue;
        }
		ReadExif = 0; // Only read exif for first image.

        if (stat(NewPic.Name, &statbuf) == -1) {
            perror(NewPic.Name);
            exit(1);
        }
        NewPic.mtime = (unsigned)statbuf.st_mtime;

        SawMotion += ProcessImage(&NewPic);

        NumProcessed += 1;
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
int DoDirectory(char * Directory)
{
    int a,b;
    Raspistill_restarted = 0;

    for (;;){
        a = DoDirectoryFunc(Directory);
        if (FollowDir){
            b = manage_raspistill(a);
            if (b) Raspistill_restarted = 1;
            sleep(1);
        }else{
            break;
        }
    }
//    if (LastPic != NULL) free(LastPic);
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
    int file_index, a, argn;

    printf("Imgcomp version 0.8 (January 2016) by Matthias Wandel\n\n");

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
    SaveDir[0] = 0;
    strcpy(SaveNames, "%m%d/%H/%m%d-%H%M%S");


    for (argn = 1; argn < argc; argn++) {
        //printf("argn = %d\n",argn);
        char * arg;
        arg = argv[argn];
        if (strcmp(arg, "-h") == 0){
            usage();
            exit (-1);
        }
    }


    // First read the configuration file.
    read_config_file();

    // Get command line arguments (which may override configuration file)
    file_index = parse_switches(argc, argv, 0);

    // Adjust region of interest to scale.
    ScaleRegion(&Regions.DetectReg, ScaleDenom);
    for (a=0;a<Regions.NumExcludeReg;a++){
        ScaleRegion(&Regions.ExcludeReg[a], ScaleDenom);
    }

    if (DoDirName[0]) printf("    Source directory = %s, follow=%d\n",DoDirName, FollowDir); 
    if (SaveDir[0]) printf("    Save to dir %s\n",SaveDir);
    if (TimelapseInterval) printf("    Timelapse interval %d seconds\n",TimelapseInterval);

    if (DiffMapFileName[0]){
        MemImage_t *MapPic;
        printf("    Diffmap file: %s\n",DiffMapFileName);
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
        if (MapPic == 0) exit(-1); // error is already reported.

        ProcessDiffMap(MapPic);
        free(MapPic);
    }

  
    if (DoDirName[0] && file_index == argc){
        // if dodir is specified in config file, but files are specified
        // on the command line, do the files instead.
        DoDirectory(DoDirName);
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
// Polling same file mode (for use with  uvccapture)
// Dynamic thresholding if too much is happening?

