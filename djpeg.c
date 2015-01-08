#include <stdio.h>
#include <ctype.h>		// to declare isupper(), tolower() 
#include <stdlib.h>

//#include "libjpeg/jconfig.h"
//#include "libjpeg/jpeglib.h"

#include "imgcomp.h"

static const char * progname;  // program name for error messages
static char * outfilename;	   // for -outfile switch

void usage (void)// complain about bad command line 
{
  fprintf(stderr, "usage: %s [switches] ", progname);
  fprintf(stderr, "inputfile outputfile\n");

  fprintf(stderr, "Switches (names may be abbreviated):\n");
  fprintf(stderr, "  -grayscale     Force grayscale output\n");
#ifdef IDCT_SCALING_SUPPORTED
  fprintf(stderr, "  -scale M/N     Scale output image by fraction M/N, eg, 1/8\n");
#endif
  fprintf(stderr, "Switches for advanced users:\n");
  fprintf(stderr, "  -nosmooth      Don't use high-quality upsampling\n");
  fprintf(stderr, "  -outfile name  Specify name for output file\n");
  fprintf(stderr, "  -verbose  or  -debug   Emit debug output\n");
  exit(-1);
}

//----------------------------------------------------------------------
// Case-insensitive matching of possibly-abbreviated keyword switches.
// keyword is the constant keyword (must be lower case already),
// minchars is length of minimum legal abbreviation.
//----------------------------------------------------------------------
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
        printf("argn = %d\n",argn);
        arg = argv[argn];
        if (*arg != '-') {
            // Not a switch, must be a file name argument 
            if (argn <= last_file_arg_seen) {
                outfilename = NULL;	
                continue;		/* ignore this name if previously processed */
            }
            break;			/* else done parsing switches */
        }
        arg++;			/* advance past switch marker character */

        if (keymatch(arg, "debug", 1) || keymatch(arg, "verbose", 1)) {
            // Enable debug printouts.  Specify more than once for more detail.
            //cinfo->err->trace_level++;

        } else if (keymatch(arg, "grayscale", 2) || keymatch(arg, "greyscale",2)) {
            // Force monochrome output.
            //cinfo->out_color_space = JCS_GRAYSCALE;
        } else if (keymatch(arg, "nosmooth", 3)) {
            // Suppress fancy upsampling
            //cinfo->do_fancy_upsampling = FALSE;

        } else if (keymatch(arg, "outfile", 4)) {
            // Set output file name.
            if (++argn >= argc)	// advance to next argument
	           usage();
            outfilename = argv[argn];	// save it away for later use

        } else if (keymatch(arg, "scale", 1)) {
            // Scale the output image by a fraction M/N. */
            if (++argn >= argc)	/* advance to next argument */
                 usage();
            //if (sscanf(argv[argn], "%d/%d", &cinfo->scale_num, &cinfo->scale_denom) != 2)
            //   usage();
        } else {
            usage();	   // bogus switch
        }
    }

    return argn;		   // return index of next arg (file name)
}

//-----------------------------------------------------------------------------------
// The main program.
//-----------------------------------------------------------------------------------
int main (int argc, char **argv)
{
    int file_index;
    progname = argv[0];

    // Scan command line to find file names.
    file_index = parse_switches(argc, argv, 0, 0);
    {
        int a;
        MemImage_t * pic;

        for (a=file_index;a<argc;a++){

            printf("input file %s\n",argv[a]);
            // Load file into memory.

            pic = LoadJPEG(argv[a], 4, 1);
            WritePpmFile("out.ppm",pic);
        }
    }

    return 0;
}
