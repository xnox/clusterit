/*  Copyright 1992 John Bovey, University of Kent at Canterbury.
 *
 *  You can do what you like with this source code as long as
 *  you don't try to make money out of it and you include an
 *  unaltered copy of this message (including the copyright).
 */

/* @(#)xsetup.h	1.1 14/7/92 (UKC) */

void init_display(int, char **, int, char **, char *);
void resize_window(void);
void switch_scrollbar(void);
void change_window_name(char *);
void change_icon_name(char *);
void error(const char *,...);
void usage(void);
