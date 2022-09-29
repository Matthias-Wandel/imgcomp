//-----------------------------------------------------------------------------------
// Tool for monitor continuously captured images for changes
// and saving changed images, as well as an image every N seconds for timelapses.
// Matthias Wandel 2015-2022
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

static int SinceMotionPix = 1000;
static int SinceMotionMs = 0;

typedef struct {
    MemImage_t *Image;
    char Name[500];
    int nind; // Name part index.
    time_t mtime;
    int DiffMag;
    int IsTimelapse;
    int IsMotion;
    int IsSkipFatigue;
}LastPic_t;

static LastPic_t LastPics[3];
static time_t NextTimelapsePix;

time_t LastPic_mtime;

static int AngleAdjusted = 0;

int FatigueSkipCountdown = 0;

//-----------------------------------------------------------------------------------
// Convert picture coordinates to relative (0-1000) on image.
//-----------------------------------------------------------------------------------
static void GeometryConvert(TriggerInfo_t  * Trig)
{
    double x,y;

    x = ((float)Trig->x/(LastPics[0].Image->width*ScaleDenom));
    y = ((float)Trig->y/(LastPics[0].Image->height*ScaleDenom));

    Trig->x = (int)((x-0.5)*1000);
    Trig->y = (int)((0.5-y)*1000);
}


//-----------------------------------------------------------------------------------
// Figure out which images should be saved.
//-----------------------------------------------------------------------------------
static int ProcessImage(LastPic_t * New, int DeleteProcessed)
{
    LastPics[2] = LastPics[1];
    LastPics[1] = LastPics[0];

    LastPics[0] = *New;
    LastPics[0].IsMotion = LastPics[0].IsTimelapse = 0;

// if lights, or motion report, also do motion detect without fatigue.
// But DoMotionRun is called from parent function to do the lights.

    TriggerInfo_t Trig;
    TriggerInfo_t Trig_nf;
    Trig_nf.DiffLevel = Trig.DiffLevel = 0;
    TriggerInfo_t* Trig_nf_p = NULL;
    if (UdpDest[0] || lighton_run[0]){
        // Also need unfatigued motion detection for triggering stuff.
        Trig_nf_p = &Trig_nf;
    }


    if (LastPics[1].Image != NULL){
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
        if (FatigueSkipCount && ++FatigueSkipCountdown >= FatigueSkipCount){
            FatigueSkipCountdown = 0;
            SkipFatigue = 1;
        }

        if (LastPics[1].Image){
            Trig = ComparePix(LastPics[1].Image, LastPics[0].Image, 1, SkipFatigue, NULL, Trig_nf_p);
        }

        LastPics[0].DiffMag = Trig.DiffLevel;
        LastPics[0].IsSkipFatigue = SkipFatigue;

        if (FollowDir){
            // When real-time following, the timestamp is more useful than the file name
            char TimeString[10];
            strftime(TimeString, 10, "%H%M%S ", localtime(&LastPic_mtime));
            fputs(TimeString,Log);
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

        if (LastPics[0].DiffMag >= Sensitivity){
            LastPics[0].IsMotion = 1;
        }

        if (SpuriousReject && LastPics[2].Image &&
            LastPics[0].IsMotion && LastPics[1].IsMotion
            && LastPics[2].DiffMag < Sensitivity/2){
            // Compare to picture before last picture.
            Trig = ComparePix(LastPics[2].Image, LastPics[0].Image, 0, 1, NULL, NULL);

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
            if (LastPics[2].IsTimelapse) KeepImage = 1;
            if (LastPics[1].IsMotion && PreMotionKeep) KeepImage |= 2;
            if (LastPics[2].IsMotion) KeepImage |= 4;
            if (SinceMotionPix <= PostMotionKeep) KeepImage |= 8;

            if (KeepImage){
                //printf(" (%s %d)",LastPics[2].Name, KeepImage);
                BackupImageFile(LastPics[2].Name, LastPics[2].DiffMag, 0);
            }
        }

        if (LastPics[2].IsMotion) SinceMotionPix = 0;

        fprintf(Log,"\n");
        SinceMotionPix += 1;


        if (UdpDest[0] && Trig_nf.DiffLevel >= Sensitivity){
            // Use un-fatigued diff level for reporting motion via UDP.
            GeometryConvert(&Trig_nf);

            printf("Send UDP motion %d,%d\n", Trig_nf.x, Trig_nf.y);

            SendUDP(Trig_nf.x, Trig_nf.y, Trig_nf.DiffLevel, Trig_nf.Motion);
        }

        Raspistill_restarted = 0;
    }

    if (LastPics[2].Image){
        // Third picture now falls out of the window.  Free it and delete it.
        free(LastPics[2].Image);
    }

    if (DeleteProcessed){
        unlink(LastPics[2].Name);
    }
    if (Trig_nf.DiffLevel >= Sensitivity || Trig.DiffLevel >= Sensitivity){
        // Return un-fatigued motion detection (if we have it) -- used for turning on lights.
        return 1;
    }
    return 0;
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
    int SawMotion;

    SawMotion = 0;

    FileNames = GetSortedDir(Directory, &NumEntries);
    if (FileNames == NULL) return 0;
    if (NumEntries == 0) return 0;

    NumProcessed = 0;

    for (a=0;a<NumEntries;a++){
        LastPic_t NewPic;
        char * ThisName;
        int l;
        time_t now = time(NULL);

        // Check that name ends in ".jpg", ".jpeg"
        ThisName = FileNames[a].FileName;
        if (ThisName[0] == 0) continue; // We already did this one.

        if (strcmp(ThisName, "angle") == 0){
			// For my hack of panning the camera with a stepper motor.
            printf("Panned, ignore next image\n");
            unlink (CatPath(Directory, "angle"));
            AngleAdjusted = 1;
        }

        l = strlen(ThisName);
        if (l < 5) continue;

        if (strcmp(FileNames[a].FileName+l-4, ".jpg") != 0 &&
                strcmp(ThisName+l-5, ".jpeg") != 0){


            if (strcmp(ThisName+l-5, ".jpg~") == 0){
                // Raspistill may leave files ending with '~' around if it was killed
                // at just the right time.  Remove these files.
                struct stat statbuf;
                char * cpn = CatPath(Directory, ThisName);
                if (stat(cpn, &statbuf) == -1) {
                    perror(ThisName);
                    continue;
                }
                if (now-statbuf.st_mtime > 5){
                    fprintf(Log, "rm temp file: %s\n",cpn);
                    unlink(cpn);
                }
            }
            continue;
        }

        strcpy(NewPic.Name, CatPath(Directory, ThisName));
        NewPic.nind = strlen(Directory)+1;


        if (strcmp(LastPics[0].Name+LastPics[0].nind, ThisName) == 0
             || strcmp(LastPics[1].Name+LastPics[1].nind, ThisName) == 0){
            // Already did this one.
            continue;
        }

        if (AngleAdjusted && DeleteProcessed){
            printf("Discard latest image (panning)\n");
            unlink(NewPic.Name);
            FileNames[a].FileName[0] = 0;
            AngleAdjusted -= 1;
            continue;
        }

        //printf("use: %s\n",ThisName);

        NewPic.Image = LoadJPEG(NewPic.Name, ScaleDenom, 0, 1);
        if (NewPic.Image == NULL){
            fprintf(Log, "Failed to load %s\n",NewPic.Name);
            if (DeleteProcessed){
                // Raspberry pi timelapse mode may at times dump a corrupt
                // picture at the end of timelapse mode.  Just delete and go on.
                unlink(NewPic.Name);
            }
            continue;
        }

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

        if (ExposureManagementOn && FollowDir && a == NumEntries-1 && now-NewPic.mtime <= 1){
            // Latest image of batch.
            // Check exposure before comparison, because we may want to restart raspistill ASAP.
            int d = CalcExposureAdjust(NewPic.Image);
            if (d){
                //fprintf(Log,"Restart raspistill for exposure adjust\n");
                relaunch_raspistill();
            }
        }

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
        DoMotionRun(a);

        int b = manage_raspistill(NumProcessed);
        if (b) Raspistill_restarted = 1;
        if (LogToFile[0] != '\0') LogFileMaintain(0);

        // Wait for more files to appear.
        struct pollfd pfd = { fd, POLLIN, 0 };
        int ret = poll(&pfd, 1, 2000);
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

    printf("Imgcomp version 0.97 (Mar 2021) by Matthias Wandel\n\n");

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
            ComparePix(pic1, pic2, 0, 0,"diff.ppm", NULL);
        }
        free(pic1);
        free(pic2);
    }else{
        int a;
        MemImage_t * pic;

        for (a=file_index;a<argc;a++){
            printf("input file %s\n",argv[a]);

            // Load file into memory.
            pic = LoadJPEG(argv[a], 4, 0, 1);

            CalcExposureAdjust(pic);
        }
    }
    return 0;
}


