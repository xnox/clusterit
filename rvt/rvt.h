/*  Copyright 1992 John Bovey, University of Kent at Canterbury.
 *
 *  You can do what you like with this source code as long as
 *  you don't try to make money out of it and you include an
 *  unaltered copy of this message (including the copyright).
 */

/* @(#)xvt.h	1.2 18/9/92 (UKC) */

#define VERSION "1.0"		/* Overall release number of the current
				 * version */

#define MARGIN 2		/* gap between the text and the window edges */

/*  Some wired in defaults so we can run without any external resources.
 */
#define DEF_FONT "8x13"
#define DEF_SAVED_LINES 64	/* number of saved lines that have scrolled of
				 * the top */
#define TERM_ENV "TERM=xterm"

/* arguments to set and reset functions.
 */
#define LOW	0
#define HIGH	1

void *cmalloc(int);
char *scopy(char *);
