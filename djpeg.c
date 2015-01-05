/*
 *	djpeg [options]  inputfile outputfile
 * In the second style, output is always to standard output, which you'd
 * normally redirect to a file or pipe to some other program.  Input is
 * either from a named file or from standard input (typically redirected).
 * The second style is convenient on Unix but is unhelpful on systems that
 * don't support pipes.  Also, you MUST use the first style if your system
 * doesn't do binary I/O to stdin/stdout.
 * To simplify script writing, the "-outfile" switch is provided.  The syntax
 *	djpeg [options]  -outfile outputfile  inputfile
 * works regardless of which command line style is used.
 */
#include "jconfig.h"
#include "libjpeg/cdjpeg.h"		/* Common decls for cjpeg/djpeg applications */


// Create the add-on message string table.
static const char * const cdjpeg_message_table[] = {
#include "libjpeg/cderror.h"
  NULL
};


static const char * progname;  // program name for error messages
static char * outfilename;	   // for -outfile switch


void usage (void)// complain about bad command line 
{
  fprintf(stderr, "usage: %s [switches] ", progname);
  fprintf(stderr, "inputfile outputfile\n");

  fprintf(stderr, "Switches (names may be abbreviated):\n");
  fprintf(stderr, "  -fast          Fast, low-quality processing\n");
  fprintf(stderr, "  -grayscale     Force grayscale output\n");
#ifdef IDCT_SCALING_SUPPORTED
  fprintf(stderr, "  -scale M/N     Scale output image by fraction M/N, eg, 1/8\n");
#endif
  fprintf(stderr, "Switches for advanced users:\n");
  fprintf(stderr, "  -nosmooth      Don't use high-quality upsampling\n");
  fprintf(stderr, "  -outfile name  Specify name for output file\n");
  fprintf(stderr, "  -verbose  or  -debug   Emit debug output\n");
  exit(EXIT_FAILURE);
}


static int parse_switches (j_decompress_ptr cinfo, int argc, char **argv,
		int last_file_arg_seen, boolean for_real)
/* Parse optional switches.
 * Returns argv[] index of first file-name argument (== argc if none).
 * Any file names with indexes <= last_file_arg_seen are ignored;
 * they have presumably been processed in a previous iteration.
 * (Pass 0 for last_file_arg_seen on the first or only iteration.)
 * for_real is FALSE on the first (dummy) pass; we may skip any expensive
 * processing.
 */
{
    int argn;
    char * arg;

    // Set up default JPEG parameters.
    outfilename = NULL;
    cinfo->err->trace_level = 0;

    /* Scan command line options, adjust parameters */

    for (argn = 1; argn < argc; argn++) {
        arg = argv[argn];
        if (*arg != '-') {
            /* Not a switch, must be a file name argument */
            if (argn <= last_file_arg_seen) {
                outfilename = NULL;	/* -outfile applies to just one input file */
                continue;		/* ignore this name if previously processed */
            }
            break;			/* else done parsing switches */
        }
        arg++;			/* advance past switch marker character */

        if (keymatch(arg, "debug", 1) || keymatch(arg, "verbose", 1)) {
            // Enable debug printouts.  Specify more than once for more detail.
            cinfo->err->trace_level++;

        } else if (keymatch(arg, "fast", 1)) {
            // Select recommended processing options for quick-and-dirty output.
            cinfo->two_pass_quantize = FALSE;
            cinfo->dct_method = JDCT_FASTEST;
            cinfo->do_fancy_upsampling = FALSE;

        } else if (keymatch(arg, "grayscale", 2) || keymatch(arg, "greyscale",2)) {
            // Force monochrome output.
            cinfo->out_color_space = JCS_GRAYSCALE;
        } else if (keymatch(arg, "nosmooth", 3)) {
            // Suppress fancy upsampling
            cinfo->do_fancy_upsampling = FALSE;

        } else if (keymatch(arg, "outfile", 4)) {
            // Set output file name.
            if (++argn >= argc)	/* advance to next argument */
	           usage();
            outfilename = argv[argn];	// save it away for later use

        } else if (keymatch(arg, "scale", 1)) {
            // Scale the output image by a fraction M/N. */
            if (++argn >= argc)	/* advance to next argument */
                 usage();
            if (sscanf(argv[argn], "%d/%d", &cinfo->scale_num, &cinfo->scale_denom) != 2)
               usage();

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
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    int file_index;
    djpeg_dest_ptr dest_mgr = NULL;
    FILE * input_file;
    FILE * output_file;
    JDIMENSION num_scanlines;

printf("hello\n");

    progname = argv[0];

    /* Initialize the JPEG decompression object with default error handling. */
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    /* Add some application-specific error messages (from cderror.h) */
    jerr.addon_message_table = cdjpeg_message_table;
    jerr.first_addon_message = JMSG_FIRSTADDONCODE;
    jerr.last_addon_message = JMSG_LASTADDONCODE;


    /* Scan command line to find file names. */
    /* It is convenient to use just one switch-parsing routine, but the switch
     * values read here are ignored; we will rescan the switches after opening
     * the input file.
     * (Exception: tracing level set here controls verbosity for COM markers
     * found during jpeg_read_header...)
     */

    file_index = parse_switches(&cinfo, argc, argv, 0, FALSE);

    /* Must have either -outfile switch or explicit output file name */
    if (outfilename == NULL) {
        if (file_index != argc-2) {
            fprintf(stderr, "%s: must name one input and one output file\n", progname);
            usage();
        }
      outfilename = argv[file_index+1];
    } else {
        if (file_index != argc-1) {
            fprintf(stderr, "%s: must name one input and one output file\n",
	            progname);
            usage();
        }
    }

    /* Open the input file. */
    if (file_index < argc) {
        if ((input_file = fopen(argv[file_index], READ_BINARY)) == NULL) {
            fprintf(stderr, "%s: can't open %s\n", progname, argv[file_index]);
            exit(EXIT_FAILURE);
        }
    } else {
        printf("Must specify input file name\n");
    }

    // Open the output file.
    if (outfilename != NULL) {
        if ((output_file = fopen(outfilename, WRITE_BINARY)) == NULL) {
            fprintf(stderr, "%s: can't open %s\n", progname, outfilename);
            exit(EXIT_FAILURE);
        }
    } else {
        printf("Must specify output file name\n");
    }

    /* Specify data source for decompression */
    jpeg_stdio_src(&cinfo, input_file);

    /* Read file header, set default decompression parameters */
    (void) jpeg_read_header(&cinfo, TRUE);

    /* Adjust default decompression parameters by re-parsing the options */
    file_index = parse_switches(&cinfo, argc, argv, 0, TRUE);

printf("hello2\n");

    dest_mgr = jinit_write_ppm(&cinfo);

    dest_mgr->output_file = output_file;
printf("hello3\n");
    // Start decompressor
    (void) jpeg_start_decompress(&cinfo);

    // Write output file header
    (*dest_mgr->start_output) (&cinfo, dest_mgr);

    // Process data 
    while (cinfo.output_scanline < cinfo.output_height) {
        num_scanlines = jpeg_read_scanlines(&cinfo, dest_mgr->buffer,
    					  dest_mgr->buffer_height);
        (*dest_mgr->put_pixel_rows) (&cinfo, dest_mgr, num_scanlines);
    }

printf("hello4\n");
    // Finish decompression and release memory.
    // I must do it in this order because output module has allocated memory
    // of lifespan JPOOL_IMAGE; it needs to finish before releasing memory.
    (*dest_mgr->finish_output) (&cinfo, dest_mgr);
    (void) jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    // Close files, if we opened them
    if (input_file != stdin) fclose(input_file);
    if (output_file != stdout) fclose(output_file);

    // All done.
    exit(jerr.num_warnings ? EXIT_WARNING : EXIT_SUCCESS);
    return 0;			//* suppress no-return-value warnings */
}
