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
static char * DoDirName = NULL;
static char * SaveDir = NULL;
static int FollowDir = 0;
static int ScaleDenom;
static Region_t DetectReg;
static int Verbosity = 0;
static int Sensitivity;
static time_t TimelapseInterval;


static time_t NextTimelapsePix;


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
    fprintf(stderr, " -f                   Do dir and monitor for new images\n");
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
    char * s;

    ScaleDenom = 4;
    DoDirName = NULL;
    Sensitivity = 10;
    DetectReg.x1 = 0;
    DetectReg.x2 = 1000000;
    DetectReg.y1 = 0;
    DetectReg.y2 = 1000000;
    TimelapseInterval = 0;

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

        } else if (keymatch(arg, "grayscale", 2)) {
            // Force monochrome output.
            //cinfo->out_color_space = JCS_GRAYSCALE;
        } else if (keymatch(arg, "savedir", 4)) {
            // Set output file name.
            if (++argn >= argc) usage();            
            SaveDir = argv[argn];
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
        } else if (keymatch(arg, "region", 1)) {
            // Specify region of interest
            if (++argn >= argc) usage();
            printf("region %s\n",argv[argn]);
            s = strstr(argv[argn], ",");
            if (s == NULL || s != argv[argn]){
                // No comma, or comma not in first position.  Parse X parameters.
                if (sscanf(argv[argn], "%d-%d", &DetectReg.x1, &DetectReg.x2) != 2) usage();
            }
            if (s != NULL){
                // Y parameters come after the comma.
                if (sscanf(s+1, "%d-%d", &DetectReg.y1, &DetectReg.y2) != 2) usage();
            }
            if (DetectReg.y2-DetectReg.y1 < 8 || DetectReg.x2-DetectReg.x1 < 8){
                fprintf(stderr,"Detect region is too small\n");
                exit(-1);
            }
            printf("Region is x:%d-%d, y:%d-%d\n",DetectReg.x1, DetectReg.x2, DetectReg.y1, DetectReg.y2);

        } else if (keymatch(arg, "dodir", 1)) {
            // Scale the output image by a fraction M/N. */
            if (++argn >= argc) usage();
            DoDirName = argv[argn];
        } else if (keymatch(arg, "f", 1)) {
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
// Concatenate dir name and file name.  Not thread safe!
//-----------------------------------------------------------------------------------
static char * CatPath(char *Dir, char * FileName)
{
    static char catpath[502];
    int pathlen;

    pathlen = strlen(Dir);
    if (pathlen > 300){
        fprintf(stderr, "path too long!");
        exit(-1);
    }
    memcpy(catpath, Dir, pathlen+1);
    if (catpath[pathlen-1] != '/' && catpath[pathlen-1] != '\\'){
        catpath[pathlen] = '/';
        pathlen += 1;
    }
    strncpy(catpath+pathlen, FileName,200);
    return catpath;
}

//-----------------------------------------------------------------------------------
// Compare file names to sort directory.
//-----------------------------------------------------------------------------------
static int fncmpfunc (const void * a, const void * b)
{
    return strcmp(*(char **)a, *(char **)b);
}

//-----------------------------------------------------------------------------------
// Read a directory and sort it.
//-----------------------------------------------------------------------------------
char ** GetSortedDir(char * Directory, int * NumFiles)
{
    char ** FileNames;
    int NumFileNames;
    int NumAllocated;
    DIR * dirp;

    NumAllocated = 5;
    FileNames = malloc(sizeof (char *) * NumAllocated);

    NumFileNames = 0;
  
    dirp = opendir(Directory);
    if (dirp == NULL){
        fprintf(stderr, "could not open dir\n");
        return NULL;
    }

    for (;;){
        struct dirent * dp;
        struct stat buf;
        int l;
        dp = readdir(dirp);
        if (dp == NULL) break;
        //printf("name: %s %d %d\n",dp->d_name, (int)dp->d_off, (int)dp->d_reclen);


        // Check that name ends in ".jpg", ".jpeg", or ".JPG", etc...
        l = strlen(dp->d_name);
        if (l < 5) continue;
        if (dp->d_name[l-1] != 'g' && dp->d_name[l-1] != 'G') continue;
        if (dp->d_name[l-2] == 'e' || dp->d_name[l-2] == 'E') l-= 1;
        if (dp->d_name[l-2] != 'p' && dp->d_name[l-2] != 'P') continue;
        if (dp->d_name[l-3] != 'j' && dp->d_name[l-3] != 'J') continue;
        if (dp->d_name[l-4] != '.') continue;
        //printf("use: %s\n",dp->d_name);

        // Check that it's a regular file.
        stat(CatPath(Directory, dp->d_name), &buf);
        if (!S_ISREG(buf.st_mode)) continue; // not a file.

        if (NumFileNames >= NumAllocated){
            //printf("realloc\n");
            NumAllocated *= 2;
            FileNames = realloc(FileNames, sizeof (char *) * NumAllocated);
        }

        FileNames[NumFileNames++] = strdup(dp->d_name);
    }
    closedir(dirp);

    // Now sort the names (could be in random order)
    qsort(FileNames, NumFileNames, sizeof(char **), fncmpfunc);

    *NumFiles = NumFileNames;
    return FileNames;
}

//-----------------------------------------------------------------------------------
// Unallocate directory structure
//-----------------------------------------------------------------------------------
void FreeDir(char ** FileNames, int NumEntries)
{
    int a;
    // Free it up again.
    for (a=0;a<NumEntries;a++){
        free(FileNames[a]);
        FileNames[a] = NULL;
    }
    free(FileNames);
}

//-----------------------------------------------------------------------------------
// Back up a photo that is of interest or applies to tiemelapse.
//-----------------------------------------------------------------------------------
char * BackupPicture(char * Directory, char * Name, char * KeepPixDir, int Threshold, int MotionTriggered)
{
    char SrcPath[500];
    static char DstPath[500];
    struct stat statbuf;
    static char ABCChar = ' ';
    static time_t LastSaveTime;
    struct tm tm;


    if (!KeepPixDir) return NULL; // Picture saving not enabled.

    strcpy(SrcPath, CatPath(Directory, Name));
    if (stat(SrcPath, &statbuf) == -1) {
        perror(SrcPath);
        exit(1);
    }
    if (!MotionTriggered){
        if (TimelapseInterval < 1) return 0; // timelapse mode off.
        if (statbuf.st_mtime < NextTimelapsePix){
            // No motion, not timelapse picture.
            return NULL;
        }
    }

    if (TimelapseInterval >= 1){
        // Figure out when the next timelapse interval should be.
        NextTimelapsePix = statbuf.st_mtime+TimelapseInterval;
        NextTimelapsePix -= (NextTimelapsePix % TimelapseInterval);
    }

    if (FollowDir){
        // In followdir mode, rename according to date.
         if (stat(SrcPath, &statbuf) == -1) {
            perror(SrcPath);
            exit(1);
        }
        if (LastSaveTime == statbuf.st_mtime){
            // If it's the same second, cycle through suffixes a-z
            ABCChar = (ABCChar >= 'a' && ABCChar <'z') ? ABCChar+1 : 'a';
        }else{
            // New time. No need for a suffix.
            ABCChar = ' ';
            LastSaveTime = statbuf.st_mtime;
        }

        tm = *localtime(&statbuf.st_mtime);
        sprintf(DstPath, "%s/%02d%02d-%02d%02d%02d%c%04d.jpg",KeepPixDir,
             tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, 
             ABCChar, Threshold);

        CopyFile(SrcPath, DstPath);
    }else{
        // In test mode, just reuse the name.
        CopyFile(SrcPath, CatPath(KeepPixDir, Name));
    }
    return DstPath;
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

    // Scan command line to find file names.
    file_index = parse_switches(argc, argv, 0, 0);

    if (FollowDir && DoDirName == NULL){
        fprintf(stderr, "Must specify directory to do and monitor with -dodir\n");
        usage();
    }
   
    if (DoDirName){
        DoDirectory(DoDirName, SaveDir);
    }

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

