/*  Copyright 1992 John Bovey, University of Kent at Canterbury.
 *
 *  You can do what you like with this source code as long as
 *  you don't try to make money out of it and you include an
 *  unaltered copy of this message (including the copyright).
 */

char xvt_xsetup_c_sccsid[] = "@(#)xsetup.c	1.1 14/7/92 (UKC)";

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#include <X11/cursorfont.h>
#include "rvt.h"
#include "command.h"
#include "xsetup.h"
#include "screen.h"
#include "sbar.h"

#define XVT_CLASS	"XTerm"
#define SBAR_WIDTH	15	/* width of scroll bar */

#define VT_EVENTS	(	ExposureMask |\
				EnterWindowMask|\
				LeaveWindowMask |\
				ButtonPressMask |\
				ButtonReleaseMask |\
				Button1MotionMask \
			)

#define MW_EVENTS	(	KeyPressMask |\
				FocusChangeMask |\
				StructureNotifyMask \
			)

#define SB_EVENTS	(	ExposureMask |\
				EnterWindowMask|\
				LeaveWindowMask |\
				Button2MotionMask |\
				ButtonReleaseMask |\
				ButtonPressMask \
			)

/*  External global variables that are initialised at startup.
 */
Display *display;
Window vt_win;			/* vt100 window */
Window sb_win;			/* scroll bar window */
Window main_win;		/* parent window */
Colormap colormap;
XFontStruct *mainfont;		/* main font structure */
XFontStruct *boldfont;		/* bold font structure */
GC gc;				/* GC for drawing text */
GC negc;			/* GC for moving areas without graphics
				 * exposure */
GC hlgc;			/* GC used for highlighting text cursor */
GC sbgc;			/* GC used for drawing the scrollbar */
unsigned long foreground;	/* foreground pixel value */
unsigned long background;	/* background pixel value */
int reverse_wrap = 0;		/* enable reverse wrapround */
int debugging = 0;		/* enable debugging output */
int messages = 0;		/* flag to enable messages */
char *username;			/* name to run as */
char *rcmd;				/* remtoe command */
char *nodename;			/* nodename */


static char *xvt_name;		/* the name the program is run under */
static char *res_name;		/* the resource name */
static char *window_name;	/* window name for titles etc. */
static char *icon_name;		/* name to display in the icon */
static int screen;		/* the X screen number */
static Visual *visual;
static XrmDatabase rDB;		/* merged resources database */
static unsigned long border;	/* border pixel value */
static int border_width = 1;
static int save_lines = DEF_SAVED_LINES;	/* number of saved lines */
static XColor foreground_color;
static XColor background_color;
static int iconic = 0;		/* start up iconized */

static int show_scrollbar = 0;	/* scroll-bar displayed if true */

#define OPTABLESIZE	21

static XrmOptionDescRec optable[] = {
	{"-display", ".display", XrmoptionSepArg, (caddr_t) NULL},
	{"-geometry", "*geometry", XrmoptionSepArg, (caddr_t) NULL},
	{"-background", "*background", XrmoptionSepArg, (caddr_t) NULL},
	{"-bg", "*background", XrmoptionSepArg, (caddr_t) NULL},
	{"-foreground", "*foreground", XrmoptionSepArg, (caddr_t) NULL},
	{"-fg", "*foreground", XrmoptionSepArg, (caddr_t) NULL},
	{"-bd", "*borderColor", XrmoptionSepArg, (caddr_t) NULL},
	{"-bw", "*borderWidth", XrmoptionSepArg, (caddr_t) NULL},
	{"-font", "*font", XrmoptionSepArg, (caddr_t) NULL},
	{"-fb", "*boldFont", XrmoptionSepArg, (caddr_t) NULL},
	{"-name", "*name", XrmoptionSepArg, (caddr_t) NULL},
	{"-sl", "*saveLines", XrmoptionSepArg, (caddr_t) NULL},
	{"-cc", "*charClass", XrmoptionSepArg, (caddr_t) NULL},
	{"-sb", "*scrollBar", XrmoptionNoArg, "on"},
	{"-rw", "*reverseWrap", XrmoptionNoArg, "on"},
	{"-msg", "*messages", XrmoptionNoArg, "on"},
	{"-iconic", "*iconic", XrmoptionNoArg, "on"},
	{"-debug", "*debug", XrmoptionNoArg, "on"},
};

static char *usearray[] = {
	"rvt [-l <username>] [-display <name>] [-geometry <spec>] [-bg <colour>]",
	"    [-fg <colour>] [-bd <colour>] [-bw <count>] [-font <fontname>]",
	"    [-fb <fontname>] [-name <name>] [-sl <count>] [-cc <char-class>]",
	"    [-sb] [-rw] [-msg] [-iconic] <remote machine name>",
	"where:",
	"<remote machine>	name of the remote machine to connect to.",
	"-l <username>		username to connect as",
	"-display <name>	specify the display (server)",
	"-geometry <spec>	the initial window geometry",
	"-background <colour>	background colour",
	"-bg <colour>		same as -background",
	"-foreground <colour>	foreground colour",
	"-fg <colour>		same as -foreground",
	"-bd <colour>		border colour",
	"-bw <count>		border width",
	"-font <fontname>	normal font",
	"-fb <fontname>		font used for bold text",
	"-name <name>		name used for matching X resources",
	"-sl <count>		number of lines saved after scrolling off window",
	"-cc <char-class>	character classes for double click",
	"-sb			provide an initial scrollbar",
	"-rw			enable reverse wrap",
	"-msg			allow messages",
	"-iconic			start up already iconized",
	NULL
};

static XSizeHints sizehints = {
	PMinSize | PResizeInc | PBaseSize,
	0, 0, 80, 24,		/* x, y, width and height */
	1, 1,			/* Min width and height */
	0, 0,			/* Max width and height */
	1, 1,			/* Width and height increments */
	{0, 0}, {0, 0},		/* Aspect ratio - not used */
	2 * MARGIN, 2 * MARGIN,	/* base size */
	0
};

static int error_handler(Display *, XErrorEvent *);
static int io_error_handler(Display *);
static char *get_resource(const char *, const char *);
static void extract_resources(char *);
static void create_window(int, char **);

/*  Error handling function, tidu up and then exit.
 */
static int
error_handler(dpy, evp)
	Display *dpy;
	XErrorEvent *evp;
{
	quit(1);
	return (0);
}

int
io_error_handler(dpy)
	Display *dpy;
{
	return(error_handler(dpy, (XErrorEvent *) NULL));
}

/*  Open the display, initialise the rDB resources database and create the
 *  window.  If title is non null then it is used as the window and icon title.
 *  iargc and iargv are the original argc, argv so the can be written to a
 *  COMMAND resource.
 */
void
init_display(argc, argv, iargc, iargv, title)
	int argc, iargc;
	char **argv, **iargv;
	char *title;
{
	char str1[256], str2[256];
	char *display_name = NULL;
	XrmDatabase commandlineDB, serverDB;
	XrmValue value;
	XGCValues gcv;
	char *str_type;
	char *s;
	int i;

	XrmInitialize();
	xvt_name = iargv[0];

	/* If there is a name argument then we need to extract it by hand so
	 * that it can be used to add other command line options. */
	res_name = NULL;
	for (i = 1; i < argc; i++)
		if (strcmp(argv[i], "-name") == 0) {
			if (argv[++i] != NULL) {
				res_name = scopy(argv[i]);
				title = scopy(argv[i]);
			} else
				error("missing -name argument");
		}
	if (res_name == NULL)
		res_name = scopy(xvt_name);

	commandlineDB = NULL;
	XrmParseCommand(&commandlineDB, optable, OPTABLESIZE, res_name, &argc, argv);

	if (argc > 1)
		usage();

	/* See if there was a display named in the command line */
	sprintf(str1, "%s.display", res_name);
	sprintf(str2, "%s.Display", XVT_CLASS);
	if (XrmGetResource(commandlineDB, str1, str2, &str_type, &value) == True) {
		strncpy(str1, value.addr, (int) value.size);
		display_name = str1;
	}
	if ((display = XOpenDisplay(display_name)) == NULL) {
		error("can't open display %s", XDisplayName(display_name));
		quit(1);
	}
	/* Get the resources from the server if there are any. */
	if ((s = XResourceManagerString(display)) != NULL) {
		serverDB = XrmGetStringDatabase(s);
		XrmMergeDatabases(serverDB, &rDB);
	}
	XrmMergeDatabases(commandlineDB, &rDB);
	screen = DefaultScreen(display);
	visual = DefaultVisual(display, screen);
	colormap = DefaultColormap(display, screen);
	extract_resources(title);

#ifdef DEBUG_X
	XSynchronize(display, True), XSetErrorHandler(abort);
#else
	if (!debugging) {
		XSetErrorHandler(error_handler);
		XSetIOErrorHandler(io_error_handler);
	}
#endif

	create_window(iargc, iargv);

	/* Create the graphics contexts. */
	gcv.foreground = foreground;
	gcv.background = background;
	gcv.font = mainfont->fid;
	gc = XCreateGC(display, main_win, GCForeground | GCBackground | GCFont, &gcv);

	gcv.graphics_exposures = False;
	negc = XCreateGC(display, main_win,
	    GCForeground | GCBackground | GCGraphicsExposures, &gcv);

	gcv.foreground = foreground;
	gcv.background = background;
	sbgc = XCreateGC(display, main_win, GCForeground | GCBackground | GCFont, &gcv);

	gcv.function = GXinvert;
	gcv.plane_mask = foreground ^ background;
	hlgc = XCreateGC(display, main_win, GCFunction | GCPlaneMask, &gcv);

	/* initialise the screen data structures. */
	scr_init(save_lines);
	sbar_init();
}
/*  Extract the named resource from the database and return a pointer to a static
 *  string containing it.
 */
static char *
get_resource(name, class)
	const char *name, *class;
{
	static char resource[256];
	char str1[256], str2[256];
	XrmValue value;
	char *str_type;

	sprintf(str1, "%s.%s", res_name, name);
	sprintf(str2, "%s.%s", XVT_CLASS, class);
	if (XrmGetResource(rDB, str1, str2, &str_type, &value) == True) {
		strncpy(resource, value.addr, (int) value.size);
		return (resource);
	}
	/* The following is added for compatibility with xterm. */
	sprintf(str1, "%s.vt100.%s", res_name, name);
	sprintf(str2, "%s.VT100.%s", XVT_CLASS, class);
	if (XrmGetResource(rDB, str1, str2, &str_type, &value) == True) {
		strncpy(resource, value.addr, (int) value.size);
		return (resource);
	}
	return (NULL);
}
/*  Extract the resource fields that are needed to open the window.
 *  if title is non-NULL it is used as a window and icon title.
 */
static void
extract_resources(title)
	char *title;
{
	char *s;
	int x, y, width, height;
	int flags;
	XColor color;

	/* First get the font since we need it to set the size. */
	if ((s = get_resource("font", "Font")) == NULL)
		s = DEF_FONT;
	if ((mainfont = XLoadQueryFont(display, s)) == NULL) {
		error("can't access font %s\n", s);
		quit(1);
	}
	sizehints.width_inc = XTextWidth(mainfont, "M", 1);
	sizehints.height_inc = mainfont->ascent + mainfont->descent;

	/* Determine whether debugging is enabled. */
	if ((s = get_resource("debug", "Debug")) != NULL)
		debugging = strcmp(s, "on") == 0;

	/* Determine whether to allow messages. */
	if ((s = get_resource("messages", "Messages")) != NULL)
		messages = strcmp(s, "on") == 0;

	/* Determine whether to start up iconized. */
	if ((s = get_resource("iconic", "Iconic")) != NULL)
		iconic = strcmp(s, "on") == 0;

	/* Determine whether to display the scrollbar. */
	if ((s = get_resource("scrollBar", "ScrollBar")) != NULL)
		show_scrollbar = strcmp(s, "on") == 0;
	if (show_scrollbar)
		sizehints.base_width += SBAR_WIDTH;

	if ((s = get_resource("borderWidth", "BorderWidth")) != NULL)
		border_width = atoi(s);
	flags = 0;
	if ((s = get_resource("geometry", "Geometry")) != NULL)
		flags = XParseGeometry(s, &x, &y, &width, &height);

	if (flags & WidthValue) {
		sizehints.width = width;
		sizehints.flags |= USSize;
	}
	if (flags & HeightValue) {
		sizehints.height = height;
		sizehints.flags |= USSize;
	}
	sizehints.width = sizehints.width * sizehints.width_inc + sizehints.base_width;
	sizehints.height = sizehints.height * sizehints.height_inc + sizehints.base_height;
	sizehints.min_width = sizehints.width_inc + sizehints.base_width;
	sizehints.min_height = sizehints.height_inc + sizehints.base_height;
	if (flags & XValue) {
		if (flags & XNegative)
			x = DisplayWidth(display, screen) + x - sizehints.width - 2 * border_width;
		sizehints.x = x;
		sizehints.flags |= USPosition;
	}
	if (flags & YValue) {
		if (flags & YNegative)
			y = DisplayHeight(display, screen) + y - sizehints.height - 2 * border_width;
		sizehints.y = y;
		sizehints.flags |= USPosition;
	}
	if (flags & XNegative)
		sizehints.win_gravity = flags & YNegative
		    ? SouthEastGravity
		    : NorthEastGravity;
	else
		sizehints.win_gravity = flags & YNegative
		    ? SouthWestGravity
		    : NorthWestGravity;
	sizehints.flags |= PWinGravity;

	/* Do the foreground, background and border colours. */
	foreground = BlackPixel(display, screen);
	XParseColor(display, colormap, "black", &foreground_color);
	if ((s = get_resource("foreground", "Foreground")) != NULL) {
		if (XParseColor(display, colormap, s, &foreground_color) == 0)
			error("invalid foreground color %s", s);
		else if (XAllocColor(display, colormap, &foreground_color) == 0)
			error("can't allocate color %s", s);
		else
			foreground = foreground_color.pixel;
	}
	background = WhitePixel(display, screen);
	XParseColor(display, colormap, "white", &background_color);
	if ((s = get_resource("background", "Background")) != NULL) {
		if (XParseColor(display, colormap, s, &background_color) == 0)
			error("invalid background color %s", s);
		else if (XAllocColor(display, colormap, &background_color) == 0)
			error("can't allocate color %s", s);
		else
			background = background_color.pixel;
	}
	border = foreground;
	if ((s = get_resource("borderColor", "BorderColor")) != NULL) {
		if (XParseColor(display, colormap, s, &color) == 0)
			error("invalid border color %s", s);
		else if (XAllocColor(display, colormap, &color) == 0)
			error("can't allocate color %s", s);
		else
			border = color.pixel;
	}
	/* Get the window and icon names */
	icon_name = title;
	window_name = title;
	if ((s = get_resource("iconName", "IconName")) != NULL) {
		icon_name = scopy(s);
		window_name = scopy(s);
	}
	if ((s = get_resource("title", "Title")) != NULL)
		window_name = scopy(s);

	if (window_name == NULL)
		window_name = scopy(res_name);
	if (icon_name == NULL)
		icon_name = scopy(res_name);

	/* Extract the bold font if there is one. */
	if ((s = get_resource("boldFont", "BoldFont")) != NULL)
		if ((boldfont = XLoadQueryFont(display, s)) == NULL)
			error("can't access font %s\n", s);

	/* Get the character class. */
	if ((s = get_resource("charClass", "CharClass")) != NULL)
		scr_char_class(s);

	/* Get the reverse wrapround flag. */
	if ((s = get_resource("reverseWrap", "ReverseWrap")) != NULL)
		reverse_wrap = strcmp(s, "on") == 0;

	/* extract xvt specific arguments. */
	if ((s = get_resource("saveLines", "SaveLines")) != NULL)
		save_lines = atoi(s);
}
/*  Open and map the window.
 */
static void
create_window(argc, argv)
	int argc;
	char **argv;
{
	XTextProperty wname, iname;
	XClassHint class;
	XWMHints wmhints;
	Cursor cursor;

	main_win = XCreateSimpleWindow(display, DefaultRootWindow(display),
	    sizehints.x, sizehints.y, sizehints.width, sizehints.height,
	    border_width, foreground, background);

	printf("%ld\n", main_win);
	fflush(NULL);

	if (XStringListToTextProperty(&window_name, 1, &wname) == 0) {
		error("cannot allocate window name");
		quit(1);
	}
	if (XStringListToTextProperty(&icon_name, 1, &iname) == 0) {
		error("cannot allocate icon name");
		quit(1);
	}
	class.res_name = res_name;
	class.res_class = XVT_CLASS;
	wmhints.input = True;
	wmhints.initial_state = iconic ? IconicState : NormalState;
	wmhints.flags = InputHint | StateHint;
	XSetWMProperties(display, main_win, &wname, &iname, argv, argc,
	    &sizehints, &wmhints, &class);
	XSelectInput(display, main_win, MW_EVENTS);

	sb_win = XCreateSimpleWindow(display, main_win, -1, -1, SBAR_WIDTH - 1,
	    sizehints.height, 1, border, background);
	cursor = XCreateFontCursor(display, XC_sb_v_double_arrow);
	XRecolorCursor(display, cursor, &foreground_color, &background_color);
	XDefineCursor(display, sb_win, cursor);
	XSelectInput(display, sb_win, SB_EVENTS);

	vt_win = XCreateSimpleWindow(display, main_win, 0, 0, sizehints.width,
	    sizehints.height, 0, border, background);
	cursor = XCreateFontCursor(display, XC_xterm);
	XRecolorCursor(display, cursor, &foreground_color, &background_color);
	XDefineCursor(display, vt_win, cursor);
	XSelectInput(display, vt_win, VT_EVENTS);

	XMapWindow(display, vt_win);

	if (show_scrollbar) {
		XMoveWindow(display, vt_win, SBAR_WIDTH, 0);
		XResizeWindow(display, vt_win, sizehints.width - SBAR_WIDTH,
			sizehints.height);
		XMapWindow(display, sb_win);
	}
	XMapWindow(display, main_win);
}
/*  Redraw the whole window after an exposure or size change.
 */
void
resize_window()
{
	Window root;
	int x, y;
	unsigned int width, height, depth;

	XGetGeometry(display, main_win, &root, &x, &y, &width, &height,
		&border_width, &depth);
	if (show_scrollbar) {
		XResizeWindow(display, sb_win, SBAR_WIDTH - 1, height);
		XResizeWindow(display, vt_win, width - SBAR_WIDTH, height);
	} else
		XResizeWindow(display, vt_win, width, height);
}
/*  Toggle scrollbar.
 */
void
switch_scrollbar()
{
	Window root;
	int x, y;
	unsigned int width, height, depth;

	XGetGeometry(display, main_win, &root, &x, &y, &width, &height,
		&border_width, &depth);
	if (show_scrollbar) {
		XUnmapWindow(display, sb_win);
		XMoveWindow(display, vt_win, 0, 0);
		width -= SBAR_WIDTH;
		sizehints.base_width -= SBAR_WIDTH;
		sizehints.width = width;
		sizehints.height = height;
		sizehints.flags = USSize | PMinSize | PResizeInc | PBaseSize;
		XSetWMNormalHints(display, main_win, &sizehints);
		XResizeWindow(display, main_win, width, height);
		show_scrollbar = 0;
	} else {
		XMapWindow(display, sb_win);
		XMoveWindow(display, vt_win, SBAR_WIDTH, 0);
		width += SBAR_WIDTH;
		sizehints.base_width += SBAR_WIDTH;
		sizehints.width = width;
		sizehints.height = height;
		sizehints.flags = USSize | PMinSize | PResizeInc | PBaseSize;
		XSetWMNormalHints(display, main_win, &sizehints);
		XResizeWindow(display, main_win, width, height);
		show_scrollbar = 1;
	}
}
/*  Change the window name displayed in the title bar.
 */
void
change_window_name(str)
	char *str;
{
	XTextProperty name;

	if (XStringListToTextProperty(&str, 1, &name) == 0) {
		error("cannot allocate window name");
		return;
	}
	XSetWMName(display, main_win, &name);
	XFree(name.value);
}
/*  Change the icon name.
 */
void
change_icon_name(str)
	char *str;
{
	XTextProperty name;

	if (XStringListToTextProperty(&str, 1, &name) == 0) {
		error("cannot allocate icon name");
		return;
	}
	XSetWMIconName(display, main_win, &name);
	XFree(name.value);
}
/*  Print an error message.
 */
/*VARARGS1*/
void
error(const char *fmt,...)
{
	va_list args;

	va_start(args, fmt);
	fprintf(stderr, "%s: ", xvt_name);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
}
/*  Print out a usage message and exit.
 */
void
usage(void)
{
	int i;

/*	fprintf(stderr, "xvt: permitted arguments are:\n"); */
	for (i = 0; usearray[i] != NULL; i++)
		fprintf(stderr, "%s\n", usearray[i]);
	exit(1);
};
