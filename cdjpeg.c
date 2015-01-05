//----------------------------------------------------------------------
// This file contains common support routines used by the IJG application
// programs (cjpeg, djpeg, jpegtran).
//----------------------------------------------------------------------

#include "libjpeg/cdjpeg.h"		/* Common decls for cjpeg/djpeg applications */
#include <ctype.h>		/* to declare isupper(), tolower() */
#ifdef USE_SETMODE
    #include <fcntl.h>		/* to declare setmode()'s parameter macros */
    /* If you have setmode() but not <io.h>, just delete this line: */
    #include <io.h>			/* to declare setmode() */
#else
    #define setmode(a, b);
#endif


//----------------------------------------------------------------------
// Case-insensitive matching of possibly-abbreviated keyword switches.
// keyword is the constant keyword (must be lower case already),
// minchars is length of minimum legal abbreviation.
//----------------------------------------------------------------------

boolean keymatch (char * arg, const char * keyword, int minchars)
{
  register int ca, ck;
  register int nmatched = 0;

  while ((ca = *arg++) != '\0') {
    if ((ck = *keyword++) == '\0')
      return FALSE;		/* arg longer than keyword, no good */
    if (isupper(ca))		/* force arg to lcase (assume ck is already) */
      ca = tolower(ca);
    if (ca != ck)
      return FALSE;		/* no good */
    nmatched++;			/* count matched characters */
  }
  /* reached end of argument; fail if it's too short for unique abbrev */
  if (nmatched < minchars)
    return FALSE;
  return TRUE;			/* A-OK */
}


//----------------------------------------------------------------------
// Routines to establish binary I/O mode for stdin and stdout.
// Non-Unix systems often require some hacking to get out of text mode.
//----------------------------------------------------------------------

FILE * read_stdin (void)
{
  FILE * input_file = stdin;

  _setmode(_fileno(stdin), O_BINARY);

#ifdef USE_FDOPEN		/* need to re-open in binary mode? */
  if ((input_file = fdopen(_fileno(stdin), READ_BINARY)) == NULL) {
    fprintf(stderr, "Cannot reopen stdin\n");
    exit(EXIT_FAILURE);
  }
#endif
  return input_file;
}


FILE * write_stdout (void)
{
  FILE * output_file = stdout;
  _setmode(_fileno(stdout), O_BINARY);

#ifdef USE_FDOPEN		/* need to re-open in binary mode? */
  if ((output_file = fdopen(_fileno(stdout), WRITE_BINARY)) == NULL) {
    fprintf(stderr, "Cannot reopen stdout\n");
    exit(EXIT_FAILURE);
  }
#endif
  return output_file;
}
