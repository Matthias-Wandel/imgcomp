//-----------------------------------------------------------------------------------
// Tool for monitor continuously captured images for changes
// and saving changed images, as well as an image every N seconds for timelapses.
// Matthias Wandel 2015-2020
//
// Imgcomp is licensed under GPL v2 (see README.txt)
//-----------------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>

#include "imgcomp.h"
#include "config.h"
#include <sys/inotify.h>
#include <poll.h>

// Configuration variables.
char * progname;  // program name for error messages
char DoDirName[200];
char SaveDir[200];
char SaveNames[200];
char TempDirName[200];
int FollowDir = 0;
int ScaleDenom;
int SpuriousReject = 0;
int PostMotionKeep = 0;
int PreMotionKeep = 0;
int wait_close_write = 0;

int BrightnessChangeRestart = 0;
int MotionFatigueTc = 30;

int FatigueSkipCount = 10; // Change to default zero.
int FatigueSkipCountdown = 0;
int FatigueGainPercent = 100;

char DiffMapFileName[200];
Regions_t Regions;

int Verbosity = 0;
char LogToFile[200];
char MoveLogNames[200];
FILE * Log;

int Sensitivity;
int Raspistill_restarted;
int TimelapseInterval;
char raspistill_cmd[200];
char blink_cmd[200];
char UdpDest[30];

//-----------------------------------------
// Tightening gap experiment hack
int GateDelay;
static int SinceMotionPix = 1000;
static int SinceMotionMs = 0;

//-----------------------------------------
// Video mode hack
int VidMode; // Video mode flag
char VidDecomposeCmd[200];


typedef struct {
    MemImage_t *Image;
    char Name[500];
    int nind; // Name part index.
    unsigned  mtime;
    int DiffMag;
    int IsTimelapse;
    int IsMotion;
    int IsSkipFatigue;
}LastPic_t;

static LastPic_t LastPics[3];
static time_t NextTimelapsePix;
static LastPic_t BaselinePic;

time_t LastPic_mtime;

//-----------------------------------------------------------------------------------
// Convert picture coordinates from 120 degree fishey lens
// to pan and tilt angles for cap shooter.
//-----------------------------------------------------------------------------------
static void GeometryConvert(TriggerInfo_t  * Trig)
{
    double x,y, magxy, mag_rad;
    double px, py,ta;
    double pan,tilt;

    // Center-referenced coordinates.
    x = (Trig->x-1920/2)/1920.0;
    y = -(Trig->y-1440/2)/1920.0;
    magxy = sqrt(x*x+y*y); // Magnitude from center.

    // Convert to degrees from center and correct for lens distortion
    mag_rad = magxy * (109 + magxy*magxy * 21);
    mag_rad = mag_rad * 3.1415/180; // Convert to radians.

    // Now convert to planar coordinates.
    ta = tan(mag_rad);
    px = (ta*x)/magxy; // px and py are in "screen" meters from center
    py = (ta*y)/magxy;
    printf("px,py = %5.2f,%5.2f  ",px,py);

    // Now convert planar coordinates to pan angle and elevation, in degrees.
    pan = atan(px)*180/3.1415;
    tilt = atan(py/sqrt(1+px*px))*180/3.1415;

    printf("Pan: %5.1f tilt:%5.1f\n",pan,tilt);
    Trig->x = (int)(pan*10); // Return it as tenth's of a degree
    Trig->y = (int)(tilt*10);
}


//-----------------------------------------------------------------------------------
// Figure out which images should be saved.
//-----------------------------------------------------------------------------------
static int ProcessImage(LastPic_t * New, int DeleteProcessed)
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
        }

        // compare with previous picture.
        Trig.DiffLevel = Trig.x = Trig.y = 0;

        int SkipFatigue = 0;
        if (++FatigueSkipCountdown >= FatigueSkipCount){
            FatigueSkipCountdown = 0;
            SkipFatigue = 1;
        }

        if (LastPics[2].Image){
            Trig = ComparePix(LastPics[1].Image, LastPics[0].Image, 1, SkipFatigue, NULL);
        }

        if (Trig.DiffLevel >= Sensitivity && PixSinceDiff > 5 && Raspistill_restarted){
            fprintf(Log,"Ignoring diff caused by raspistill restart\n");
            Trig.DiffLevel = 0;
        }
        LastPics[0].DiffMag = Trig.DiffLevel;
        LastPics[0].IsSkipFatigue = SkipFatigue;

        if (FollowDir){
            // When real-time following, the timestamp is more useful than the file name
            char TimeString[10];
            strftime(TimeString, 10, "%H%M%S ", localtime(&LastPic_mtime));
            fprintf(Log,TimeString);
        }else{
            fprintf(Log,"%s: ",LastPics[0].Name+LastPics[0].nind);
        }
        if (Trig.DiffLevel){
            fprintf(Log,"%4d", Trig.DiffLevel);
            if (Trig.DiffLevel*5 >= Sensitivity){
                fprintf(Log," (%4d,%4d)", Trig.x, Trig.y);
            }
            SinceMotionMs = 0;
        }

        if (LastPics[0].DiffMag > Sensitivity){
            LastPics[0].IsMotion = 1;
        }

        if (SpuriousReject && LastPics[2].Image &&
            LastPics[0].IsMotion && LastPics[1].IsMotion
            && LastPics[2].DiffMag < Sensitivity/2){
            // Compare to picture before last picture.
            Trig = ComparePix(LastPics[2].Image, LastPics[0].Image, 0, 1, NULL);

            //printf("Diff with pix before last: %d\n",Trig.DiffLevel);
            if (Trig.DiffLevel < Sensitivity){
                // An event that was just one frame.  We assume this was something
                // spurious, like an insect or a rain drop or a camera glitch.
                printf(" (spurious %d, ignore)", Trig.DiffLevel);
                LastPics[0].IsMotion = 0;
                LastPics[1].IsMotion = 0;
            }
        }
        if (LastPics[0].IsMotion){
            fprintf(Log," (motion)");
            if (LastPics[0].IsSkipFatigue) fprintf(Log, " (sf)");
        }
        if (LastPics[0].IsTimelapse) fprintf(Log," (time)");

        
        if (SaveDir[0]){
            int KeepImage = 0;
            if (LastPics[2].IsTimelapse) {KeepImage = 1; printf(" time"); }
            if (LastPics[2].IsMotion) {KeepImage = 1; printf(" mot"); }
            if (LastPics[1].IsMotion && PreMotionKeep) {KeepImage = 1; printf(" pre");}
            if (SinceMotionPix < PostMotionKeep) {KeepImage = 1; printf(" post");}
            
            if (KeepImage){
                BackupImageFile(LastPics[2].Name, LastPics[2].DiffMag, 0);
            }
        }
        
        if (LastPics[1].IsMotion) SinceMotionPix = 0;

        fprintf(Log,"\n");
        SinceMotionPix += 1;


        if (Trig.DiffLevel > Sensitivity && UdpDest[0]){
            // For my cap shooter experiment.  Not useful for anything else.
            char showx[1001];
            int xs, a;
            const int colwidth=120;

            for (a=0;a<colwidth;a++){
                showx[a] = '.';
            }
            showx[colwidth] = '\0';
            xs = (Trig.x*colwidth)/(1920);
            if (xs < 0) xs = 0;
            if (xs > colwidth-3) xs = colwidth-3;

            showx[xs] = '#';
            showx[xs+1] = '#';
            printf("%s %d,%d\n",showx, Trig.x, Trig.y);

            GeometryConvert(&Trig);

            if (UdpDest[0]) SendUDP(Trig.x, Trig.y, Trig.DiffLevel, Trig.Motion);
        }

        Raspistill_restarted = 0;
    }

    if (LastPics[2].Image){
        static int PrintFlag;
        // Third picture now falls out of the window.  Free it and delete it.

        if (UdpDest[0] && (SinceMotionMs > 4000 || (SinceMotionMs > 2000 && !BaselinePic.Image))){
            // If it's been 30 seconds since we saw motion, save this image
            // as a background image for later squirrel detection
            if (BaselinePic.Image){
                free(BaselinePic.Image);
            }
            BaselinePic = LastPics[2];
            if (!PrintFlag){
                printf("Baselining\n");
                PrintFlag = 1;
            }
        }else{
            free(LastPics[2].Image);
            if (SinceMotionMs < 1000) PrintFlag = 0;
        }
    }

    if (DeleteProcessed){
        unlink(LastPics[2].Name);
    }
    return LastPics[0].IsMotion;
}

//-----------------------------------------------------------------------------------
// Process a whole directory of files.
//-----------------------------------------------------------------------------------
static int NumProcessed;
static int DoDirectoryFunc(char * Directory, int DeleteProcessed)
{
    DirEntry_t * FileNames;
    int NumEntries;
    int a;
    int ReadExif;
    int SawMotion;

    SawMotion = 0;

    FileNames = GetSortedDir(Directory, &NumEntries);
    if (FileNames == NULL) return 0;
    if (NumEntries == 0) return 0;

    ReadExif = 1;
    NumProcessed = 0;
    for (a=0;a<NumEntries;a++){
        // Don't redo old pictures that we have looked at, but
        // not yet deleted because we may still need them.
        if (strcmp(LastPics[0].Name+LastPics[0].nind, FileNames[a].FileName) == 0
           || strcmp(LastPics[1].Name+LastPics[1].nind, FileNames[a].FileName) == 0){
            // Zero out file name to indicate skip this one.
            FileNames[a].FileName[0] = 0;
        }
    }


    for (a=0;a<NumEntries;a++){
        LastPic_t NewPic;
        char * ThisName;
        int l;

        // Check that name ends in ".jpg", ".jpeg", or ".JPG", etc...
        ThisName = FileNames[a].FileName;
        if (ThisName[0] == 0) continue; // We already did this one.

        l = strlen(ThisName);
        if (l < 5) continue;
        if (ThisName[l-1] != 'g' && ThisName[l-1] != 'G') continue;
        if (ThisName[l-2] == 'e' || ThisName[l-2] == 'E') l-= 1;
        if (ThisName[l-2] != 'p' && ThisName[l-2] != 'P') continue;
        if (ThisName[l-3] != 'j' && ThisName[l-3] != 'J') continue;
        if (ThisName[l-4] != '.') continue;
        //printf("use: %s\n",ThisName);

        strcpy(NewPic.Name, CatPath(Directory, ThisName));
        NewPic.nind = strlen(Directory)+1;

        NewPic.Image = LoadJPEG(NewPic.Name, ScaleDenom, 0, ReadExif);
        if (NewPic.Image == NULL){
            fprintf(Log, "Failed to load %s\n",NewPic.Name);
            if (DeleteProcessed){
                // Raspberry pi timelapse mode may at times dump a corrupt
                // picture at the end of timelapse mode.  Just delete and go on.
                unlink(NewPic.Name);
            }
            continue;
        }
        ReadExif = 0; // Only read exif for first image.

        if (ThisName[0] == 's' && ThisName[1] == 'f' && ThisName[2] >= '0' && ThisName[2] <= '9'){
            // Video decomposed files have no meaningful timestamp,
            // but filename starts with 'sf' and contains unix time minus 1 billion.
            NewPic.mtime = atoi(ThisName+2) + (time_t)1000000000;
        }else{
            struct stat statbuf;
            if (stat(NewPic.Name, &statbuf) == -1) {
                perror(NewPic.Name);
                exit(1);
            }
            NewPic.mtime = (unsigned)statbuf.st_mtime;
        }
        LastPic_mtime = NewPic.mtime;

        SawMotion += ProcessImage(&NewPic, DeleteProcessed);

        NumProcessed += 1;
    }

    FreeDir(FileNames, NumEntries); // Free up the whole directory structure.
    FileNames = NULL;

    SinceMotionMs += 1000;

    return SawMotion;
}

//-----------------------------------------------------------------------------------
// Process a whole directory of jpeg files.
//-----------------------------------------------------------------------------------
int DoDirectory(char * Directory)
{
    int a;
    Raspistill_restarted = 0;

    if (!FollowDir){
        // Offline mode - just a one shot, no polling.
        return DoDirectoryFunc(Directory, FollowDir);
    }

    int wd, fd;
    fd = inotify_init();
    if (fd < 0) perror("inotify_init");

    // raspistill first writes under a temporary file name, ending with "~" then renames.
    // IN_MOVED_TO triggers when the file is renamed to the final file name.
    // But when using ffmpeg for the RTSP camera hack, ffmpeg writes the files under
    // the original name, so IN_CLOSE_WRITE is needed for that.
    wd = inotify_add_watch( fd, Directory, wait_close_write ? IN_CLOSE_WRITE : IN_MOVED_TO);
    if (wd < 0){
        fprintf(Log, "add watch failed\n");
        return 0;
    }

    for (;;){
        a = DoDirectoryFunc(Directory, FollowDir);
        int b = manage_raspistill(NumProcessed);
        if (b) Raspistill_restarted = 1;
        if (LogToFile[0] != '\0') LogFileMaintain(0);

        // Wait for more files to appear.
        struct pollfd pfd = { fd, POLLIN, 0 };
        int ret = poll(&pfd, 1, 1100);
        if (ret < 0) {
            fprintf(Log, "select failed: %s\n", strerror(errno));
            sleep(1);
            continue;
        }
        if (ret == 0){
            // Timeout waiting for a new file.
            fprintf(Log, "wait file poll() timeout\n");
            continue;
        }

        // Read and ignore the event to clear it out.
        const int EVENT_BUF_LEN = 200;
        char buffer[EVENT_BUF_LEN];
        read( fd, buffer, EVENT_BUF_LEN );
    }

    return a;
}

//-----------------------------------------------------------------------------------
// Process a whole directory of video files.
//-----------------------------------------------------------------------------------
int DoDirectoryVideos(char * DirName)
{
    int a,ret;
    int infileindex;
    int alt = 0;

    Raspistill_restarted = 0;
    infileindex = strstr(VidDecomposeCmd, "<infile>")-VidDecomposeCmd;

    if (infileindex <= 0){
        fprintf(stderr, "Must specify '<infile>' as part of videodecomposecmd\n");
        exit(-1);
    }

    for (;;){
        DirEntry_t * FileNames;
        int NumEntries;
        char VidFileName[200];
        char FFCmd[300];
        int Saw_motion;
        time_t now;
        int VideoActive = 0;

        NumProcessed = 0;
        alt += 1;

        FileNames = GetSortedDir(DirName, &NumEntries);
        if (FileNames == NULL){
            fprintf(stderr, "Could not read dir %s\n",DirName);
            return 0;
        }

        now = time(NULL);
        if (NumEntries != 1){
            fprintf(Log,"%d files to process",NumEntries);
            fprintf(Log, "(%02d:%02d)...\n",(int)(now%3600/60), (int)(now%60));
        }
        for (a=0;a<NumEntries;a++){
            int age;

            age = (int)(now-FileNames[a].MTime);
            if (age < 1){
                VideoActive = 1; // Video files getting updated.
                if (alt & 1 || NumEntries > 1){
                    // Print waiting only every other time to cut down on log clutter.
                    fprintf(Log, "Vid '%s' aged %d %dk (Wait)\n",FileNames[a].FileName, age, FileNames[a].FileSize>>10);
                }
                continue;
            }else{
                fprintf(Log,"Vid '%s': %dk Extract keyframes (%02d:%02d)...\n", FileNames[a].FileName, FileNames[a].FileSize>>10,
                    (int)(now%3600)/60, (int)(now%60));
            }

            strcpy(VidFileName, CatPath(DirName, FileNames[a].FileName));
            strncpy(FFCmd, VidDecomposeCmd, infileindex);
            FFCmd[infileindex] = 0;
            strcpy(FFCmd+infileindex, VidFileName);
            strcat(FFCmd, VidDecomposeCmd+infileindex+8);

            // Use timestamp of video file to sequence output file names (so we'll have the respective times for those)
            unsigned seq = (unsigned)FileNames[a].MTime - 1000000000;
            sprintf(FFCmd+strlen(FFCmd), " -start_number %u %s/sf%%d.jpg",seq, TempDirName);

            errno = 0;
            ret = system(FFCmd);
            if (ret || errno){
                // A command can however fail without errno getting set or system returning an error.
                if (errno) perror("system");
                fprintf(Log, "Error on command %s\n",FFCmd);
                continue;
            }

            // Now should have some files in temp dir.
            Saw_motion = DoDirectoryFunc(TempDirName, 1);
            if (Saw_motion){
                char * DstName;
                char * Ext;
                fprintf(Log,"Vid has motion %d\n", Saw_motion);
                // Make filename, but don't copy the file.
                DstName = BackupImageFile(VidFileName, Saw_motion,1);

                Ext = strstr(DstName, ".h264");
                if (Ext) strcpy(Ext, ".mp4"); // Change xtension to .mp4

                if (DstName){
                    sprintf(FFCmd,"MP4Box -add %s \"%s\"",VidFileName, DstName);
                    errno = 0;
                    ret = system(FFCmd);
                    if (ret || errno){
                        if (errno) perror("system");
                        fprintf(Log, "Error on command %s\n",FFCmd);
                        continue;
                    }
                }
            }

            if (FollowDir){
                //fprintf(Log, "Delete video %s\n",VidFileName);
                unlink(VidFileName);
            }
        }
        FreeDir(FileNames, NumEntries); // Free up the whole directory structure.

        if (FollowDir){
            int b = manage_raspistill(VideoActive);
            if (b) Raspistill_restarted = 1;
            if (LogToFile[0] != '\0') LogFileMaintain(0);
            sleep(1);
        }else{
            break;
        }

    }
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

    Log = stdout;

    printf("Imgcomp version 0.95 (Jun 2020) by Matthias Wandel\n\n");

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
    strcpy(SaveNames, "%y%m%d/%H/%m%d-%H%M%S");
    LastPic_mtime = time(NULL); // Log names are based on this time, need before images.

    // Default configuration file to look for
    char *config_file = "imgcomp.conf";

    for (argn = 1; argn < argc; argn++) {
        //printf("argn = %d\n",argn);
        char * arg;
        arg = argv[argn];
        if (strcmp(arg, "-h") == 0){
            usage();
            exit (-1);
        } else if(strcmp(arg, "-configfile") == 0) {
            // Allow multiple config files by specifying them with -c configfilename.
            // This silently fails (does not override the config file name) if the
            // -configfile is the last argument.
            if(argv[argn+1]) config_file = argv[argn+1];
        }
    }

    // First read the configuration file.
    read_config_file(config_file);

    // Get command line arguments (which may override configuration file)
    file_index = parse_switches(argc, argv, 0);

    if (LogToFile[0] != '\0'){
        Log = NULL;
        LogFileMaintain(0);
    }

    if (UdpDest[0]) InitUDP(UdpDest);

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


    // These directories are likely to be on ramdisk, so they may need re-creating.
    if (FollowDir) EnsurePathExists(DoDirName,0);
    if (TempDirName[0]) EnsurePathExists(TempDirName,0);



    if (DoDirName[0] && file_index == argc){
        // if dodir is specified in config file, but files are specified
        // on the command line, do the files instead.
        if (!VidMode){
            DoDirectory(DoDirName);
        }else{
            if (TempDirName[0] == 0){
                fprintf(stderr, "must specify tempdir for video mode\n");
                exit(-1);
            }
            DoDirectoryVideos(DoDirName);
        }
    }

    if (argc-file_index == 2){
        MemImage_t *pic1, *pic2;

        printf("load %s\n",argv[file_index]);
        pic1 = LoadJPEG(argv[file_index], ScaleDenom, 0, 0);
        printf("\nload %s\n",argv[file_index+1]);
        pic2 = LoadJPEG(argv[file_index+1], ScaleDenom, 0, 0);
        if (pic1 && pic2){
            Verbosity = 2;
            ComparePix(pic1, pic2, 0, 0,"diff.ppm");
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

