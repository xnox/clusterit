/*  Copyright 1992 John Bovey, University of Kent at Canterbury.
 *
 *  Redistribution and use in source code and/or executable forms, with
 *  or without modification, are permitted provided that the following
 *  condition is met:
 *
 *  Any redistribution must retain the above copyright notice, this
 *  condition and the following disclaimer, either as part of the
 *  program source code included in the redistribution or in human-
 *  readable materials provided with the redistribution.
 *
 *  THIS SOFTWARE IS PROVIDED "AS IS".  Any express or implied
 *  warranties concerning this software are disclaimed by the copyright
 *  holder to the fullest extent permitted by applicable law.  In no
 *  event shall the copyright-holder be liable for any damages of any
 *  kind, however caused and on any theory of liability, arising in any
 *  way out of the use of, or inability to use, this software.
 *
 *  -------------------------------------------------------------------
 *
 *  In other words, do not misrepresent my work as your own work, and
 *  do not sue me if it causes problems.  Feel free to do anything else
 *  you wish with it.
 */

char xvt_sbar_c_sccsid[] = "@(#)sbar.c	1.2 16/11/93 (UKC)";

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#include "rvt.h"
#include "xsetup.h"
#include "command.h"
#include "ttyinit.h"
#include "sbar.h"

/*  External global variables that are initialised at startup.
 */
extern Display		*display;
extern Window		sb_win;		/* scroll bar window */
extern GC 		sbgc;

static unsigned int width;	/* scrollbar width */
static unsigned int height;	/* scrollbar height */
static int mtop;	/* top of marked area */
static int mbot;	/* bottom of marked area */

/*  Most recent arguments to sbar_show
 */
static int last_length = 100;	/* initial values to give a full bar */
static int last_low = 0;
static int last_high = 100;

static unsigned char stipple_bits[] = {
	0x55, 0x55,
	0xaa, 0xaa,
	0x55, 0x55,
	0xaa, 0xaa,
	0x55, 0x55,
	0xaa, 0xaa,
	0x55, 0x55,
	0xaa, 0xaa,
	0x55, 0x55,
	0xaa, 0xaa,
	0x55, 0x55,
	0xaa, 0xaa,
	0x55, 0x55,
	0xaa, 0xaa,
	0x55, 0x55,
	0xaa, 0xaa,
};

/*  Initialise scrollbar data structures - called just once.
 */
void
sbar_init()
{
	Pixmap stipple;
	XGCValues gcv;

	stipple = XCreateBitmapFromData(display,sb_win,stipple_bits,16,16);
	if (stipple == 0) {
		error("Cannot create scrollbar bitmap");
		quit(1);
	}
	gcv.fill_style = FillOpaqueStippled;
	gcv.stipple = stipple;
	XChangeGC(display,sbgc,GCFillStyle|GCStipple,&gcv);
	sbar_reset();
}

/*  Redraw the scrollbar after a size change
 */
void
sbar_reset()
{
	Window root;
	int x, y;
	unsigned int border_width, depth;

	XGetGeometry(display,sb_win,&root,&x,&y,&width,&height,&border_width,&depth);
	mbot = -1;	/* force a redraw */
	sbar_show(last_length,last_low,last_high);
}

/*  Redraw the scrollbar to show the area from low to high proportional to length.
 */
void
sbar_show(length,low,high)
int length, low, high;
{
	int top, bot;

	if (length == 0)
		return;

	last_length = length;
	last_low = low;
	last_high = high;

	top = height - 1 - height * high / length;
	bot = height - 1 - height * low / length;

	if (top == mtop && bot == mbot)
		return;
	if (top > 0)
		XClearArea(display,sb_win,0,0,width,top - 1,False);
	if (bot >= top)
		XFillRectangle(display,sb_win,sbgc,0,top,width,bot - top + 1);
	if (bot < height - 1)
		XClearArea(display,sb_win,0,bot + 1,width,height - bot - 1,False);
}
