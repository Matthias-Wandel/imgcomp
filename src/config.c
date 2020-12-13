//-----------------------------------------------------------------------------------
// Read imgcomp.conf configuration file.
// Matthias Wandel 2015
//
// Imgcomp is licensed under GPL v2 (see README.txt)
//-----------------------------------------------------------------------------------
#include <stdio.h>
#include <ctype.h>      // to declare isupper(), tolower() 
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "imgcomp.h"
#include "config.h"

//-----------------------------------------------------------------------------------
// Indicate command line usage.
//-----------------------------------------------------------------------------------
void usage (void)// complain about bad command line 
{
    fprintf(stderr, "usage: %s [switches] ", progname);
    fprintf(stderr, "inputfile outputfile\n");

    fprintf(stderr, 
     "Switches (names may be abbreviated):\n"
     " -configfile file      Override default imgcomp.conf file name\n"
     " -scale   N            Scale before detection by 1/N.  Default 1/4\n"
     " -region  x1-x2,y1-y2  Specify region of interest\n"
     " -exclude x1-x2,y1-y2  Exclude from area of interest\n"
     " -diffmap <filename>   A file to use as diff map\n"
     " -dodir   <srcdir>     Compare images in dir, in order\n"
     " -followdir <srcdir>   Do dir and monitor for new images\n"
     " -savedir <saveto>     Where to save images with changes\n"
     " -savenames <scheme>   Output naming scheme.  Uses strftime\n"
     " -premotion <n>        0 or 1.  Keep up to 1 image before motion\n"
     " -postmotion <n>       Keep n frames after motion was detected\n"
     " -tempdir <dir>        Where to put temp images for video mode hack\n"
     " -sensitivity N        Set motion sensitivity. Lower=more sensitive\n"
     " -blink_cmd <command>  Run this command when motion detected\n"
     "                       (used to blink the camera LED)\n"
     " -tl N                 Save image every N seconds regardless\n"
     " -spurious             Ignore any change that returns to\n"
     "                       previous image in the next frame\n"
     
     " -aquire_cmd <command> Raspistill command line and options.\n"
     "                       -o option will be appended to this\n"
     " -exmanage <1>         When set to 1, imgcomp takes over camera exposure\n"
     "                       settings based on analyzing image and restarts\n"
     "                       raspistill with new settings when light levels change\n"
     "                       When not specified, raspistill does auto exposure\n"
     " -iso <min>-<max>      With -exmanage 1, Limits of ISO settings to use with\n"
     "                       raspistill.  Must be 25-6400\n"
     " -exposure <min>-<max> With -exmanage 1, Limits to shutter speeds (in seconds)\n"
     "                       to use with raspistill.  0.0001-0.25\n"
     " -isooverextime <val>  with -exmanage 1, sets the ISO/shutter speed curve\n"
     "                       Unless limits are hit, imgcomp will aim for ISO divided\n"
     "                       by exposure time to be this value.  Default 16000\n"
     "                       Larger means shorter shutter speeds, smaller means lower\n"
     "                       ISO (less grainy) but slower shutter, more motion blur\n"
     " -pixxat <val>         with -exmanage 1, Jpeg pixel value at which image\n"
     "                       saturates because camera modules v1 and v2 saturate\n"
     "                       before hitting 255.  Defaults to value appropriate for\n"
     "                       camera module detected\n"
     " -fatigue_tc           Motion fatigue time constant, 0=no motion fatigue\n"
     " -fatigue_percent <n>  Gain factor (default 100) for motion fatigue strength\n"
     " -fatigue_skip <n>     Skip applying motion fatigue every n frames\n"
     " -verbose or -debug    Emit more verbose output\n"
     " -logtofile            Log to file instead of stdout\n"
     " -movelognames <schme> Rotate log files, scheme works just like\n"
     "                       it does for savenames\n"
     " -sendudp <ipaddr>     Send UDP packets for motion detection\n"
     " -wait_close_write 1   Wait for IN_CLOSE_WRITE rather than IN_CREATE\n"
     " -relaunch_timeout     Timeout (in seconds) before giving up on capture\n"
     "                       command (raspistill) and re-launching\n"
     " -give_up_timeout      Timeout (in seconds) before giving up completely and\n"
     "                       attempting to reboot (set to zero to disable)\n"
     );
   
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
        if ((ck = *keyword++) == '\0') return 0;// arg longer than keyword, no good
        if (isupper(ca))  ca = tolower(ca);     // force arg to lcase (assume ck is already)
        if (ca != ck) return 0;                 // no good
        nmatched++;                             // count matched characters
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
        fprintf(stderr, "Parameter '%s' needs to be followed by a value\n",tag);
        return -1;
    }

    if (keymatch(tag, "spurious", 4)) {
        SpuriousReject = value[0]-'0';
        if ((SpuriousReject != 0 && SpuriousReject != 1) || value[1] != 0){
            fprintf(stderr, "Spurious value can only be 0 or 1\n");
        }
    }else if (keymatch(tag, "configfile", 10)) {
        // do nothing; this arg was already interpreted earlier.  Also this is meaningless
        // inside a config file.
    }else if (keymatch(tag, "postmotion", 10)) {
        if (sscanf(value, "%d", &PostMotionKeep) != 1) return -1;
    }else if (keymatch(tag, "premotion", 9)) {
        if (sscanf(value, "%d", &PreMotionKeep) != 1) return -1;
        if (PreMotionKeep != 0 && PreMotionKeep != 1){
            fprintf(stderr, "preMotion may only be 0 or 1\n");
            return -1;
        }
    }else if (keymatch(tag, "brmonitor", 4)) {
        // exposure monnitor works a lot like how brmonitor used to work.
        if (sscanf(value, "%d", &ExposureManagementOn) != 1) return -1;
    }else if (keymatch(tag, "exmanage", 5)) {
        if (sscanf(value, "%d", &ExposureManagementOn) != 1) return -1;
    }else if (keymatch(tag, "relaunch_timeout", 16)) {
        if (sscanf(value, "%d", &relaunch_timeout) != 1) return -1;        
    }else if (keymatch(tag, "give_up_timeout", 15)) {
        if (sscanf(value, "%d", &give_up_timeout) != 1) return -1;
    }else if (strcmp(tag, "fatigue") == 0 || keymatch(tag, "fatigue_tc", 10)) {
        if (sscanf(value, "%d", &MotionFatigueTc) != 1) return -1;
    }else if (keymatch(tag, "fatigue_percent", 11)) {
        if (sscanf(value, "%d", &FatigueGainPercent) != 1) return -1;
    }else if (keymatch(tag, "fatigue_skip", 10)) {
        if (sscanf(value, "%d", &FatigueSkipCount) != 1) return -1;
    } else if (keymatch(tag, "scale", 5)) {
        // Scale the output image by a fraction 1/N.
        if (sscanf(value, "%d", &ScaleDenom) != 1) return -1;
    } else if (keymatch(tag, "sensitivity", 5)) {
        // Sensitivity level (lower = more senstitive) 
        if (sscanf(value, "%d", &Sensitivity) != 1) return -1;
    } else if (keymatch(tag, "timelapse", 5)) {
        // Scale the output image by a fraction M/N.
        if (sscanf(value, "%d", (int *)&TimelapseInterval) != 1) return -1;
    } else if (keymatch(tag, "aquire_cmd", 4)) {
        // Set the command for raspistill command.
        strncpy(raspistill_cmd, value, sizeof(raspistill_cmd)-1);
    } else if (keymatch(tag, "iso", 3)) {
        int n = sscanf(value, "%d-%d", &ex.ISOmin, &ex.ISOmax);
        if (n != 1 && n != 2) return -1;
        if (n == 1) ex.ISOmax = ex.ISOmin;
        if (ex.ISOmin > ex.ISOmax || ex.ISOmax > 6400 || ex.ISOmin < 10){
            fprintf(stderr, "Bad ISO range.  Must be 10 - 6400\n");
            return -1;
        }
    } else if (keymatch(tag, "exposure", 3)) {
        int n = sscanf(value, "%f-%f", &ex.Tmin, &ex.Tmax);
        if (n != 1 && n != 2) return -1;
        if (n == 1) ex.Tmax = ex.Tmin;
        if (ex.Tmin > ex.Tmax || ex.Tmax > 0.25 || ex.Tmin < 0.00009){
            // Timelapses mode with more than 1/4 second exposure time tends to mess up
            // raspistill, some combinations hard locking up the whole Pi.
            fprintf(stderr, "Bad exposure time values.  Must be 0.0001 - 10 (seconds)\n");
            return -1;
        }
    } else if (keymatch(tag, "pixsat", 7)) {
        if (sscanf(value, "%d", &ex.SatVal) != 1) return -1;
        if (ex.SatVal < 50 || ex.SatVal > 255){
            fprintf(stderr, "Pixel saturation must be between in range of 50-255\n");
            return -1;
        }
    } else if (keymatch(tag, "isooverextime", 5)) {
        if (sscanf(value, "%d", &ex.ISOoverExTime) != 1) return -1;
        if (ex.ISOoverExTime < 1000 || ex.ISOoverExTime > 50000){
            fprintf(stderr, "ISO/Exposure is out of range (100-50000)\n");
            return -1;
        }
        
    } else if (keymatch(tag, "blink_cmd", 5)) {
        // Obsolete blink LED feature.
        strncpy(blink_cmd, value, sizeof(blink_cmd)-1);
    } else if (keymatch(tag, "savedir", 4)) {
        // Set where output goes
        strncpy(SaveDir,value, sizeof(SaveDir)-1);
    } else if (keymatch(tag, "savenames", 5)) {
        // Set file / directory naming scheme.  Do not chagne from default
        // if you want the HTML browsing system to still work.
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

    } else if (keymatch(tag, "logtofile", 8)) {
        // Log to a file instead of stdout.  Must log to /ramdisk/log.txt for realtime view to work.
        strncpy(LogToFile,value, sizeof(SaveDir)-1);
    } else if (keymatch(tag, "movelognames", 12)) {
        // Where to backup logs to.
        strncpy(MoveLogNames,value, sizeof(SaveDir)-1);
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
        strncpy(DoDirName, value, sizeof(DoDirName)-1);
        FollowDir = 0;
    } else if (keymatch(tag, "followdir", 6)) {
        strncpy(DoDirName,value, sizeof(DoDirName)-1);
        FollowDir = 1;
    } else if (keymatch(tag, "tempdir", 6)) {
        strncpy(TempDirName,value, sizeof(DoDirName)-1);
    } else if (keymatch(tag, "vidmode", 7)) {
        if (sscanf(value, "%d", &VidMode) != 1) return -1;
    } else if (keymatch(tag, "viddecomposecmd", 15)) {
        strncpy(VidDecomposeCmd, value, sizeof(VidDecomposeCmd)-1);
    } else if (keymatch(tag, "sendudp", 7)) {
        strncpy(UdpDest,value, sizeof(UdpDest)-1);
    } else if (keymatch(tag, "wait_close_write", 16)) {
        if (sscanf(value, "%d", &wait_close_write) != 1) return -1;
    }else{
        fprintf(stderr,"argument '%s' not understood\n",tag);
        return -1;     // bogus switch
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
int parse_switches (int argc, char **argv, int last_file_arg_seen)
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
void read_config_file(char *config_file)
{
    FILE * file;
    char ConfLine[201];
    int linenum = 0;

    file = fopen(config_file, "r");
    if (file == NULL){
        fprintf(stderr, "No configuration file %s\n",config_file);
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
            fprintf(stderr, "Error on line %d of imgcomp.conf\n",linenum);
            exit(-1);
        }

    }
}
