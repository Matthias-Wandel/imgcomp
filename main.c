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
static Region_t DetectReg;
static int Verbosity = 0;
static int Sensitivity;
int TimelapseInterval;


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
//    fprintf(stderr, " -exclude x1-x2,y1-y2 Exclude part of region\n");
    fprintf(stderr, " -dodir   <srcdir>    Compare images in dir, in order\n");
    fprintf(stderr, " -followdir <srcdir>  Do dir and monitor for new images\n");
    fprintf(stderr, " -savedir <saveto>    Where to save images with changes\n");
    fprintf(stderr, " -sens N              Set sensitivity\n");
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
static int keymatch (char * arg, const char * keyword, int minchars)
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
// Parse command line switches
// Returns argv[] index of first file-name argument (== argc if none).
//-----------------------------------------------------------------------------------
static int parse_switches (int argc, char **argv, int last_file_arg_seen, int for_real)
{
    int argn;
    char * arg;

    // Scan command line options, adjust parameters
    for (argn = 1; argn < argc; argn++) {
        //printf("argn = %d\n",argn);
        arg = argv[argn];
        if (*arg != '-') {
            return argn;
            usage();
        }
        arg++;		// advance past switch marker character

        if (keymatch(arg, "debug", 1) || keymatch(arg, "verbose", 1)) {
            // Enable debug printouts.  Specify more than once for more detail.
            Verbosity += 1;

        } else if (keymatch(arg, "scale", 1)) {
            // Scale the output image by a fraction M/N.
            if (++argn >= argc) usage();            
            if (sscanf(argv[argn], "%d", &ScaleDenom) != 1)
               usage();
        } else if (keymatch(arg, "sens", 1)) {
            // Scale the output image by a fraction M/N.
            if (++argn >= argc) usage();            
            if (sscanf(argv[argn], "%d", &Sensitivity) != 1)
               usage();
        } else if (keymatch(arg, "tl", 1)) {
            // Scale the output image by a fraction M/N.
            if (++argn >= argc) usage();            
            if (sscanf(argv[argn], "%d", (int *)&TimelapseInterval) != 1)
               usage();
            if (TimelapseInterval < 1){
                fprintf(stderr,"timelaps interval must be at least 1 second\n");
                exit(-1);
            }
        } else if (keymatch(arg, "savedir", 4)) {
            // Set output file name.
            if (++argn >= argc) usage();            
            strncpy(SaveDir,argv[argn], sizeof(SaveDir)-1);

        } else if (keymatch(arg, "dodir", 1)) {
            // Scale the output image by a fraction M/N. */
            if (++argn >= argc) usage();
            strncpy(DoDirName,argv[argn], sizeof(DoDirName)-1);
        } else if (keymatch(arg, "followdir", 1)) {
            if (++argn >= argc) usage();
            strncpy(DoDirName,argv[argn], sizeof(DoDirName)-1);
            FollowDir = 1;

        } else {
            usage();	   // bogus switch
        }
    }

    // Adjust region of interest to scale.
    DetectReg.x1 /= ScaleDenom;
    DetectReg.x2 /= ScaleDenom;
    DetectReg.y1 /= ScaleDenom;
    DetectReg.y2 /= ScaleDenom;

    return argn;  // return index of next arg (file name)
}

//-----------------------------------------------------------------------------------
// Too many parameters for imgcomp running.  Just read them from a configuration file.
//-----------------------------------------------------------------------------------
static void read_config_file()
{
    FILE * file;
    char ConfLine[201];

    // Reset to default parameters.
    ScaleDenom = 4;
    DoDirName[0] = '\0';
    Sensitivity = 10;
    DetectReg.x1 = 0;
    DetectReg.x2 = 1000000;
    DetectReg.y1 = 0;
    DetectReg.y2 = 1000000;
    TimelapseInterval = 0;

    file = fopen("imgcomp.conf", "r");
    if (file == NULL){
        fprintf(stderr, "Could not open configuration file imgcomp.conf\n");
        exit(-1);
    }
    for(;;){
        char *s, *v, *t;
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

        v = strstr(s, "=");
        if (v == NULL){
            fprintf(stderr, "Configuration lines must be in format 'tag=value', not '%s'\n",s);
            exit(-1);
        }
        t = v;

        // Remove value leading spaces
        v += 1;
        while (*v == ' ' || *v == '\t') v++;

        // remove tag trailing spaces.
        *t = '\0';
        while (*t == ' ' || *t == '\t'){
            *t = '\0';
            t--;
        }

        // Now finally have the tag extracted.
        printf("'%s' = '%s'\n",s, v);


        if (strcmp(s,"scale") == 0){
            if (sscanf(v, "%d", &ScaleDenom) != 1) goto bad_value;

        }else if(strcmp(s,"region") == 0){
            // Specify region of interest
            t = strstr(v, ",");
            if (t == NULL || t != v){
                // No comma, or comma not in first position.  Parse X parameters.
                if (sscanf(v, "%d-%d", &DetectReg.x1, &DetectReg.x2) != 2) goto bad_value;
            }
            if (v != NULL){
                // Y parameters come after the comma.
                if (sscanf(v+1, "%d-%d", &DetectReg.y1, &DetectReg.y2) != 2) goto bad_value;
            }
            if (DetectReg.y2-DetectReg.y1 < 16 || DetectReg.x2-DetectReg.x1 < 16){
                fprintf(stderr,"Detect region is too small\n");
                exit(-1);
            }
            printf("Region is x:%d-%d, y:%d-%d\n",DetectReg.x1, DetectReg.x2, DetectReg.y1, DetectReg.y2);

        }else if(strcmp(s,"dodir") == 0){
            strncpy(DoDirName,v, sizeof(DoDirName)-1);
        }else if(strcmp(s,"followdir") == 0){
            strncpy(DoDirName,v, sizeof(DoDirName)-1);
            FollowDir = 1;
        }else if(strcmp(s,"savedir") == 0){
            strncpy(SaveDir,v, sizeof(SaveDir)-1);

        }else if(strcmp(s,"aquire_cmd") == 0){
            strncpy(raspistill_cmd,v, sizeof(200)-1);

        }else if(strcmp(s,"sensitivity") == 0){
            if (sscanf(v, "%d", &Sensitivity) != 1) goto bad_value;
        }else if(strcmp(s,"verbose") == 0){
            if (sscanf(v, "%d", &Verbosity) != 1) goto bad_value;
        }else if(strcmp(s,"timelapse") == 0){
            if (sscanf(v, "%d", (int *)&TimelapseInterval) != 1){
bad_value:
                fprintf(stderr, "Value of %s=%s\n not understood\n",s,v);
            }
        }else {
            printf("Unknown tag '%s=%s'\n",s,v);
        }
    }
}



static MemImage_t *LastPic;
static char LastPicName[500];
static int LastDiffMag;
static char * LastPicSaveName = NULL;

//-----------------------------------------------------------------------------------
// Process a whole directory of files.
// Return 
//-----------------------------------------------------------------------------------
static int DoDirectoryFunc(char * Directory, char * KeepPixDir, int Delete)
{
    char ** FileNames;
    int NumEntries;
    int a;
    int ReadExif;
    int NumProcessed;

    static MemImage_t *CurrentPic;
    static char * CurrentPicName;

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
            printf("Pix %s vs %s:",LastPicName, CurrentPicName);
            diff = ComparePix(LastPic, CurrentPic, DetectReg, NULL, Verbosity);
            printf(" %d ",diff);

            if (diff >= Sensitivity){
                if (!LastPicSaveName){
                    // Save the pre-motion pic if it wasn't already saved.
                    BackupPicture(Directory, LastPicName, KeepPixDir, LastDiffMag, 1);
                }
                LastPicSaveName = BackupPicture(Directory, CurrentPicName, KeepPixDir, diff, 1);
                if (LastPicSaveName) printf("  Motion pic %s",LastPicSaveName);
            }else{
                LastPicSaveName = BackupPicture(Directory, CurrentPicName, KeepPixDir, diff, 0);
                if (LastPicSaveName) printf("  Timelapse pic %s",LastPicSaveName);
            }
            if (!LastPicSaveName) printf("                ");
            printf("\n");
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

    return NumProcessed;
}


//-----------------------------------------------------------------------------------
// Process a whole directory of files.
//-----------------------------------------------------------------------------------
int DoDirectory(char * Directory, char * KeepPixDir)
{
    int a;

    for (;;){
        a = DoDirectoryFunc(Directory, KeepPixDir, FollowDir);
        if (FollowDir){
            manage_raspistill(a);
            sleep(1);
        }else{
            break;
        }
    }
    if (LastPic != NULL) free(LastPic);
    return a;
}


//-----------------------------------------------------------------------------------
// The main program.
//-----------------------------------------------------------------------------------
int main(int argc, char **argv)
{
    int file_index;
    progname = argv[0];

    // First read the configuration file.
    read_config_file();

    // Get command line arguments (which may override configuration file)
    file_index = parse_switches(argc, argv, 0, 0);
  
    if (DoDirName[0]) DoDirectory(DoDirName, SaveDir);

    if (argc-file_index == 2){
        MemImage_t *pic1, *pic2;
        
        printf("load %s\n",argv[file_index]);
        pic1 = LoadJPEG(argv[file_index], ScaleDenom, 0, 0);
        printf("\nload %s\n",argv[file_index+1]);
        pic2 = LoadJPEG(argv[file_index+1], ScaleDenom, 0, 0);
        if (pic1 && pic2){
            ComparePix(pic1, pic2, DetectReg, "diff.ppm", 2);
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

