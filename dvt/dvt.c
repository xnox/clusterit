/* $Id: dvt.c,v 1.4 2004/10/04 18:23:27 garbled Exp $ */
/*
 * Copyright (c) 1998, 1999, 2000
 *	Tim Rightnour.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Tim Rightnour.
 * 4. The name of Tim Rightnour may not be used to endorse or promote 
 *    products derived from this software without specific prior written 
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TIM RIGHTNOUR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL TIM RIGHTNOUR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/keysym.h>
#include <X11/Xproto.h>

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>

#include "Down.xbm"
#include "Up.xbm"

#include "../common/common.h"

#if !defined(lint) && defined(__NetBSD__)
__COPYRIGHT(
"@(#) Copyright (c) 1998, 1999, 2000\n\
        Tim Rightnour.  All rights reserved\n");
__RCSID("$Id: dvt.c,v 1.4 2004/10/04 18:23:27 garbled Exp $");
#endif /* not lint */

#ifndef __P
#define __P(protos) protos
#endif

void do_command __P((int, char *));
void sig_handler __P((int));
void setup_xlib __P((void));
void getGC __P((GC *));
void load_font __P((XFontStruct **));
int x_error_handler __P((Display *, XErrorEvent *));

/* globals */
int debug, errorflag, gotsigint, gotsigterm, exclusion, grouping;
node_t *nodelink;
group_t *grouplist;
char **rungroup;
char **lumplist;
pid_t currentchild;
char *progname;
Display *display;
int screen;
Window textwin, quitwin, sbwin;
Window sbup, sbdown;
XSizeHints size_hints;
GC gc;
XFontStruct *font_info;
Window win;
const char *window_name = "dvt";
const char *icon_name = "dvt";
char *display_name = NULL;
Pixmap up_pic, down_pic;

int (*old_handler)(Display *, XErrorEvent *);

/* 
 *  dsh is a cluster management tool based upon the IBM tool of the
 *  same name.  It allows a user, or system administrator to issue
 *  commands in paralell on a group of machines.
 */

int
main(int argc, char **argv)
{
    extern char	*optarg;
    extern int	optind;

    int someflag, ch, i, fanout, showflag, fanflag;
    char *p, *q, *group, *nodename, *username;
    char **exclude, **grouptemp;
    struct rlimit	limit;
    node_t *nodeptr;

    someflag = showflag = fanflag = 0;
    exclusion = debug = errorflag = 0;
    gotsigint = gotsigterm = grouping = 0;
    fanout = DEFAULT_FANOUT;
    nodename = NULL;
    username = NULL;
    group = NULL;
    nodeptr = NULL;
    nodelink = NULL;

    rungroup = malloc(sizeof(char **) * GROUP_MALLOC);
    if (rungroup == NULL)
	bailout();
    exclude = malloc(sizeof(char **) * GROUP_MALLOC);
    if (exclude == NULL)
	bailout();

    progname = p = q = argv[0];
    while (progname != NULL) {
	q = progname;
	progname = (char *)strsep(&p, "/");
    }
    progname = strdup(q);

#if defined(__linux__)
    while ((ch = getopt(argc, argv, "+?deiqf:g:l:w:x:")) != -1)
#else
	while ((ch = getopt(argc, argv, "?deiqf:g:l:w:x:")) != -1)
#endif
	    switch (ch) {
	    case 'd':		/* we want to debug dsh (hidden)*/
		debug = 1;
		break;
	    case 'e':		/* we want stderr to be printed */
		errorflag = 1;
		break;
	    case 'i':		/* we want tons of extra info */
		debug = 1;
		break;
	    case 'l':		/* invoke me as some other user */
		username = strdup(optarg);
		break;
	    case 'q':		/* just show me some info and quit */
		showflag = 1;
		break;
	    case 'f':		/* set the fanout size */
		fanout = atoi(optarg);
		fanflag = 1;
		break;
	    case 'g':		/* pick a group to run on */
		i = 0;
		grouping = 1;
		for (p = optarg; p != NULL; ) {
		    group = (char *)strsep(&p, ",");
		    if (group != NULL) {
			if (((i+1) % GROUP_MALLOC) != 0) {
			    rungroup[i++] = strdup(group);
			} else {
			    grouptemp = realloc(rungroup,
				i*sizeof(char **) +
				GROUP_MALLOC*sizeof(char *));
			    if (grouptemp != NULL)
				rungroup = grouptemp;
			    else
				bailout();
			    rungroup[i++] = strdup(group);
			}
		    }
		}
		group = NULL;
		break;			
	    case 'x':		/* exclude nodes, w overrides this */
		exclusion = 1;
		i = 0;
		for (p = optarg; p != NULL; ) {
		    nodename = (char *)strsep(&p, ",");
		    if (nodename != NULL) {
			if (((i+1) % GROUP_MALLOC) != 0) {
			    exclude[i++] = strdup(nodename);
			} else {
			    grouptemp = realloc(exclude,
				i*sizeof(char **) +
				GROUP_MALLOC*sizeof(char *));
			    if (grouptemp != NULL)
				exclude = grouptemp;
			    else
				bailout();
			    exclude[i++] = strdup(nodename);
			}
		    }
		}
		break;
	    case 'w':		/* perform operation on these nodes */
		someflag = 1;
		for (p = optarg; p != NULL; ) {
		    nodename = (char *)strsep(&p, ",");
		    if (nodename != NULL)
			(void)nodealloc(nodename);
		}
		break;
	    case '?':		/* you blew it */
		(void)fprintf(stderr,
		    "usage: %s [-eiq] [-f fanout] [-g rungroup1,...,rungroupN] "
		    "[-l username] [-x node1,...,nodeN] [-w node1,..,nodeN]\n",
		    progname);
		return(EXIT_FAILURE);
		/*NOTREACHED*/
		break;
	    default:
		break;
	    }

    if (!fanflag)
/* check for a fanout var, and use it if the fanout isn't on the commandline */
	if (getenv("FANOUT"))
	    fanout = atoi(getenv("FANOUT"));

    if (!someflag)
	parse_cluster(exclude);

    argc -= optind;
    argv += optind;
    if (showflag) {
	do_showcluster(fanout);
	return(EXIT_SUCCESS);
    }

    /*
     * set per-process limits for max descriptors, this avoids running
     * out of descriptors and the odd errors that come with that.
     */
    if (getrlimit(RLIMIT_NOFILE, &limit) != 0)
	bailout();
    if (limit.rlim_cur < fanout * 5) {
	limit.rlim_cur = fanout * 5;
	if (setrlimit(RLIMIT_NOFILE, &limit) != 0)
	    bailout();
    }

    setup_xlib();
    do_command(fanout, username);
    return(EXIT_SUCCESS);
}

#define MAX_POPUP_STRING_LENGTH 11
#define MAX_MAPPED_STRING_LENGTH 11

    void
    setup_xlib(void)
{
    unsigned int x = 0, y = 0; /* window position */
    unsigned int border_width = 4;  /* four pixels */
    unsigned int width, height;
    unsigned int twidth, theight;

    /* connect to X server */
	
    if ( (display=XOpenDisplay(display_name)) == NULL )
    {
	(void)fprintf(stderr, "dvt: cannot connect to X server %s\n",
	    XDisplayName(display_name));
	exit(EXIT_FAILURE);
    }

    screen = DefaultScreen(display);
    width = 142;
    height = 88;
    win = XCreateSimpleWindow(display, RootWindow(display,screen), x, y,
	width, height, border_width, BlackPixel(display, screen),
	WhitePixel(display,screen));

    size_hints.flags = PPosition | PSize | PMinSize;
    size_hints.x = x;
    size_hints.y = y;
    size_hints.width = width;
    size_hints.height = height;
    size_hints.min_width = 142;
    size_hints.min_height = 88;

    XSetStandardProperties(display, win, window_name, icon_name,
	(Pixmap)NULL, NULL, 0, &size_hints);
    XSelectInput(display, win, ExposureMask | KeyPressMask | ButtonPressMask
	| StructureNotifyMask | ButtonReleaseMask);
    load_font(&font_info);
    getGC(&gc);
    XMapWindow(display, win);

    twidth = MAX_POPUP_STRING_LENGTH * font_info->max_bounds.width + 4;
    theight = font_info->max_bounds.ascent +
	font_info->max_bounds.descent + 4;
    x = 0;
    y = height - theight - 8;
    textwin = XCreateSimpleWindow(display, win, x, y, twidth, theight,
	border_width, BlackPixel(display,screen), WhitePixel(display, screen));
    XSelectInput(display, textwin,
	ExposureMask | KeyPressMask | ButtonReleaseMask);
    XDefineCursor(display, textwin, XCreateFontCursor(display, XC_xterm));
    XMapWindow(display, textwin);

    twidth = 4 * font_info->max_bounds.width + 4;
    theight = font_info->max_bounds.ascent +
	font_info->max_bounds.descent + 4;
    x = 0;
    y = 0;
    quitwin = XCreateSimpleWindow(display, win, x, y, twidth, theight,
	border_width, BlackPixel(display,screen), WhitePixel(display, screen));
    XSelectInput(display, quitwin, ExposureMask | ButtonPressMask);
    XDefineCursor(display, quitwin, XCreateFontCursor(display, XC_pirate));
    XMapWindow(display, quitwin);

    y = font_info->max_bounds.ascent + font_info->max_bounds.descent + 4 + 9;
    x = MAX_POPUP_STRING_LENGTH * font_info->max_bounds.width + 4 + 9;
    theight = Down_height;
    twidth = Down_width;
    sbup = XCreateSimpleWindow(display, win, x, y, twidth, theight,
	border_width-2, BlackPixel(display,screen),
	WhitePixel(display, screen));
    XSelectInput(display, sbup, ExposureMask | ButtonPressMask);
    XDefineCursor(display, sbup, XCreateFontCursor(display, XC_sb_up_arrow));
    XMapWindow(display, sbup);

    y += theight + 5;
    sbdown = XCreateSimpleWindow(display, win, x, y, twidth, theight,
	border_width-2, BlackPixel(display,screen),
	WhitePixel(display, screen));
    XSelectInput(display, sbdown, ExposureMask | ButtonPressMask);
    XDefineCursor(display, sbdown,
	XCreateFontCursor(display, XC_sb_down_arrow));
    XMapWindow(display, sbdown);

    up_pic = XCreatePixmapFromBitmapData(display, sbup, Up_bits, Up_width,
	Up_height, BlackPixel(display,screen), WhitePixel(display, screen),
	DefaultDepth(display, screen));

    down_pic = XCreatePixmapFromBitmapData(display, sbdown, Down_bits,
	Down_width, Down_height, BlackPixel(display,screen),
	WhitePixel(display, screen), DefaultDepth(display, screen));

    twidth = 9 * font_info->max_bounds.width + 4;
    theight = font_info->max_bounds.ascent +
	font_info->max_bounds.descent + 4;
    x = 5 * font_info->max_bounds.width + 4;
    y = 0;
    sbwin = XCreateSimpleWindow(display, win, x, y, twidth, theight,
	border_width, BlackPixel(display,screen), WhitePixel(display, screen));
    XSelectInput(display, sbwin, ExposureMask | ButtonPressMask);
    XDefineCursor(display, sbwin, XCreateFontCursor(display, XC_hand1));
    XMapWindow(display, sbwin);

    old_handler = XSetErrorHandler(x_error_handler);
}

void
getGC(ingc)
    GC *ingc;
{
    unsigned long valuemask = 0; /* ignore XGCvalues and use defaults */
    XGCValues values;
    unsigned int line_width = 6;
    int line_style = LineOnOffDash;
    int cap_style = CapRound;
    int join_style = JoinRound;
    int dash_offset = 0;
    static char dash_list[] = {12, 24};
    int list_length = 2;

    /* Create default Graphics Context */
    *ingc = XCreateGC(display, win, valuemask, &values);

    /* specify font */
    XSetFont(display, *ingc, font_info->fid);

    /* specify black foreground since default may be white on white */
    XSetForeground(display, *ingc, BlackPixel(display,screen));

    /* set line attributes */
    XSetLineAttributes(display, *ingc, line_width, line_style, cap_style,
	join_style);

    /* set dashes to be line_width in length */
    XSetDashes(display, *ingc, dash_offset, dash_list, list_length);
}

void
load_font(infont_info)
    XFontStruct **infont_info;
{
    const char *fontname = "9x15";

    /* Access font */
    if ((*infont_info = XLoadQueryFont(display, fontname)) == NULL)
    {
	(void) fprintf( stderr, "Basic: Cannot open 9x15 font\n");
	exit( -1 );
    }
}

/* 
 * Do the actual dirty work of the program, now that the arguments
 * have all been parsed out.
 */

void
do_command(fanout, username)
    char *username;
    int fanout;
{
    struct sigaction signaler;
    FILE *fd, *in;
    char pipebuf[2048];
    int count, status, i, j, g, l, piping;
    size_t maxnodelen;
    char *rsh, *cd;
    node_t *nodeptr, *nodehold;
    XEvent report;
    XComposeStatus compose;
    KeySym keysym;
    int bufsize=MAX_POPUP_STRING_LENGTH;
    char buffer[MAX_POPUP_STRING_LENGTH] = "";
    char string[MAX_POPUP_STRING_LENGTH] = "";
    int start_x, start_y, cur_letter;

    maxnodelen = 0;
    j = i = 0;
    piping = 0;
    in = NULL;
    cd = pipebuf;
    start_x = 2;
    cur_letter = 0;
    start_y = font_info->max_bounds.ascent + 2;

    if (debug) {
	if (username != NULL)
	    (void)printf("As User: %s\n", username);
	(void)printf("On nodes:\n");
    }
    for (nodeptr = nodelink; nodeptr; nodeptr = nodeptr->next) {
	if (strlen(nodeptr->name) > maxnodelen)
	    maxnodelen = strlen(nodeptr->name);
	if (debug) {
	    if (!(j % 4) && j > 0)
		(void)printf("\n");
	    (void)printf("%s\t", nodeptr->name);
	}
	j++;
    }

    i = j; /* side effect of above */
    j = i / fanout;
    if (i % fanout)
	j++; /* compute the # of rungroups */

    if (j > 1) {
	(void)fprintf(stderr,
	    "You are attempting to run this command on a group of nodes\n"
	    "that is larger than the fanout value of %d.  This will likely\n"
	    "open far too many windows for your machine to handle.\n"
	    "If you would like to override this, you may set the fanout\n"
	    "level higher when running this command.\n", fanout);
	exit(EXIT_FAILURE);
    }

    if (debug) {
	(void)printf("\nFanout: %d Groups:%d\n", fanout, j);
    }
/*	close(STDIN_FILENO);  DAMN this bug took awhile to find */

    signaler.sa_handler = sig_handler;
    signaler.sa_flags |= SA_RESTART;
    sigaction(SIGTERM, &signaler, NULL);
    sigaction(SIGINT, &signaler, NULL);
    g = 0;
    nodeptr = nodelink;
    nodehold = nodeptr;
    if (gotsigterm || gotsigint)
	exit(EXIT_FAILURE);
    for (i=0; (i < fanout && nodeptr != NULL); i++) {
	g++;
	if (gotsigterm)
	    exit(EXIT_FAILURE);
	if (debug)
	    (void)printf("Working node: %d, fanout part: %d\n",
		g, i);
/*
 * we set up pipes for each node, to prepare for the oncoming barrage of data.
 * Close on exec must be set here, otherwise children spawned after other
 * children, inherit the open file descriptors, and cause the pipes to remain
 * open forever.
 */
	if (pipe(nodeptr->out.fds) != 0)
	    bailout();
	if (pipe(nodeptr->err.fds) != 0)
	    bailout();
	if (fcntl(nodeptr->out.fds[0], F_SETFD, 1) == -1)
	    bailout();
	if (fcntl(nodeptr->out.fds[1], F_SETFD, 1) == -1)
	    bailout();
	if (fcntl(nodeptr->err.fds[0], F_SETFD, 1) == -1)
	    bailout();
	if (fcntl(nodeptr->err.fds[1], F_SETFD, 1) == -1)
	    bailout();
	nodeptr->childpid = fork();
	switch (nodeptr->childpid) {
	    /* its the ol fork and switch routine eh? */
	case -1:
	    bailout();
	    break;
	case 0:
	    /* remove from parent group to avoid kernel
	     * passing signals to children.
	     */
	    (void)setsid();
	    if (piping)
		if (close(STDIN_FILENO) != 0)
		    bailout();
	    if (dup2(nodeptr->out.fds[1], STDOUT_FILENO) 
		!= STDOUT_FILENO) 
		bailout();
	    if (dup2(nodeptr->err.fds[1], STDERR_FILENO)
		!= STDERR_FILENO)
		bailout();
	    if (close(nodeptr->out.fds[0]) != 0)
		bailout();
	    if (close(nodeptr->err.fds[0]) != 0)
		bailout();
	    rsh = getenv("RVT_CMD");
	    if (rsh == NULL)
		rsh = strdup("rvt");
	    if (rsh == NULL)
		bailout();
	    if (debug)
		(void)printf("%s %s\n", rsh, nodeptr->name);
	    if (username != NULL)
/* interestingly enough, this -l thing works great with ssh */
		execlp(rsh, rsh, "-l", username, nodeptr->name,
		    (char *)0);
	    else
		execlp(rsh, rsh, nodeptr->name, (char *)0);
	    bailout();
	    break;
	default:
	    break;
	} /* switch */
	nodeptr = nodeptr->next;
    } /* for i */
    nodeptr = nodehold;
    for (i=0; (i < fanout && nodeptr != NULL); i++) {
	if (gotsigterm)
	    exit(EXIT_FAILURE);
	if (debug)
	    (void)printf("Printing node: %d, "
		" fanout part: %d\n", g-fanout+i, i);
	currentchild = nodeptr->childpid;
	/* now close off the useless stuff, and read the goodies */
	if (close(nodeptr->out.fds[1]) != 0)
	    bailout();
	if (close(nodeptr->err.fds[1]) != 0)
	    bailout();
	fd = fdopen(nodeptr->out.fds[0], "r"); /* stdout */
	if (fd == NULL)
	    bailout();
	while ((cd = fgets(pipebuf, sizeof(pipebuf), fd))) {
	    if (cd != NULL) {
		nodeptr->win_id = atoi(cd);
		if (debug)
		    (void)printf("WINDOW ID is 0x%x\n",
			(unsigned int)nodeptr->win_id);
		break;
	    }
	}
	fclose(fd);
	nodeptr = nodeptr->next;
    } /* for pipe read */
    for (;;) {
	/* put the x goop right here */
	XNextEvent(display, &report);
	switch (report.type) {
	case Expose:
	    if (report.xexpose.window == textwin)
		XDrawString(display, textwin, gc, start_x, start_y, string,
		    strlen(string));
	    else if (report.xexpose.window == quitwin)
		XDrawString(display, quitwin, gc, start_x, start_y, "Quit", 4);
	    else if (report.xexpose.window == sbwin)
		XDrawString(display, sbwin, gc, start_x, start_y,
		    "ScrollBar", 9);
	    else if (report.xexpose.window == sbup)
		XCopyArea(display, up_pic, sbup, gc, 0, 0, Up_width,
		    Up_height, 0, 0);
	    else if (report.xexpose.window == sbdown)
		XCopyArea(display, down_pic, sbdown, gc, 0, 0, Down_width,
		    Down_height, 0, 0);
	    break;
	case ButtonPress:
	    if (report.xbutton.window == quitwin) {
		for (nodeptr = nodehold; nodeptr != NULL;
		     nodeptr = nodeptr->next) {
		    kill(nodeptr->childpid, SIGHUP);
		    fd = fdopen(nodeptr->err.fds[0], "r"); /* stderr */
		    if (fd == NULL)
			bailout();
		    while ((cd = fgets(pipebuf, sizeof(pipebuf), fd))) {
			if (errorflag && cd != NULL)
			    (void)printf("ERROR %*s: %s",
				-maxnodelen, nodeptr->name, cd);
		    }
		    fclose(fd);
		    (void)waitpid(nodeptr->childpid, &status, 0);
		}
		XUnloadFont(display, font_info->fid);
		XFreeGC(display, gc);
		XCloseDisplay(display);
		exit(EXIT_SUCCESS);
	    } else if (report.xbutton.window == sbwin) {
		report.xbutton.state = ControlMask; /* add the control key */
		for (nodeptr = nodehold; nodeptr != NULL;
		     nodeptr = nodeptr->next) {
		    if (nodeptr->win_id != 0)
			XSendEvent(display, nodeptr->win_id, False,
			    0, &report);
		}
	    } else if (report.xbutton.window == sbup) {
		report.xbutton.state = Mod5Mask; /* add the mod5 mask */
		report.xbutton.button = Button3; /* set the button */
		for (nodeptr = nodehold; nodeptr != NULL;
		     nodeptr = nodeptr->next) {
		    if (nodeptr->win_id != 0)
			XSendEvent(display, nodeptr->win_id, False,
			    0, &report);
		}
	    } else if (report.xbutton.window == sbdown) {
		report.xbutton.state = Mod5Mask; /* add the mod5 mask */
		report.xbutton.button = Button1; /* set the button */
		for (nodeptr = nodehold; nodeptr != NULL;
		     nodeptr = nodeptr->next) {
		    if (nodeptr->win_id != 0)
			XSendEvent(display, nodeptr->win_id, False,
			    0, &report);
		}
	    }
	    break;
	case ButtonRelease:
	    if (report.xbutton.window == textwin ||
		report.xbutton.window == win) {
		if (report.xbutton.button == Button2)
		    for (nodeptr = nodehold; nodeptr != NULL;
			 nodeptr = nodeptr->next) {
			if (nodeptr->win_id != 0)
			    XSendEvent(display, nodeptr->win_id, False,
				0, &report);
		    }
	    }
	    break;
	case KeyPress:
	    count = XLookupString(&report.xkey, buffer, bufsize, &keysym,
		&compose);
	    if (strlen(string) + strlen(buffer) >= MAX_POPUP_STRING_LENGTH) {
		cur_letter = 0;
		XClearWindow(display, textwin);
		(void)memset(string, '\0', MAX_POPUP_STRING_LENGTH);
	    }

	    for (l = 0; (cur_letter < MAX_POPUP_STRING_LENGTH
		     && l < strlen(buffer)); l++, cur_letter++) {
		string[cur_letter] = buffer[l];
	    }

	    XDrawString(display, textwin, gc, start_x, start_y, string,
		strlen(string));

	    for (nodeptr = nodehold; nodeptr != NULL;
		 nodeptr = nodeptr->next) {
		if (nodeptr->win_id != 0)
		    XSendEvent(display, nodeptr->win_id, False, KeyPressMask,
			&report);
	    }
	    buffer[1] = '\0';
	    break;
	default:
	    break;
	}
	/* X GOOP */
    } /* infinate loop */
}

int
x_error_handler(dpy, evp)
    Display *dpy;
    XErrorEvent *evp;
{
    node_t *nodeptr;

    /* disconnect a lost window */
    if (evp->error_code == BadWindow && evp->request_code == X_SendEvent) {
	for (nodeptr = nodelink; nodeptr != NULL; nodeptr = nodeptr->next)
	    if (nodeptr->win_id == evp->resourceid)
		nodeptr->win_id = 0;
    } else

	old_handler(dpy, evp);
    return(0);
}

void
sig_handler(i)
    int i;
{
    switch (i) {
    case SIGINT:
	killpg(currentchild, SIGINT);
	break;
    case SIGTERM:
	gotsigterm = 1;
	break;
    default:
	bailout();
	break;
    }
}
