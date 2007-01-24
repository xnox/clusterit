/*  Copyright 1992-94, 1997 John Bovey, University of Kent at Canterbury.
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

char xvt_ttyinit_c_sccsid[] = "@(#)ttyinit.c	1.3 11/1/94 (UKC)";

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <utmp.h>
#include <grp.h>
#include <pwd.h>
#include <errno.h>
#include <string.h>
#ifdef __NetBSD__
#include <util.h>
#endif
#ifdef __linux__
#include <pty.h>
#endif
#include "rvt.h"
#include "token.h"
#include "command.h"
#include "ttyinit.h"
#include "screen.h"
#include "xsetup.h"

/*  Global variables that are set up at the beginning and then not changed
 */
extern Display		*display;
extern int 		comm_fd;	/* file descriptor connected to the command */
extern int		fd_width;	/* Number of available file descriptors */

static int comm_pid = -1;		/* process id of child */
static char *ttynam = NULL;

/*  Catch a SIGCHLD signal and exit if the direct child has died.
 */
static void
catch_child(int sig)
{
	int status;

	if (wait(&status) == comm_pid)
		quit(0);

	signal(SIGCHLD,catch_child);
}

/*  Catch a fatal signal and tidy up before quitting
 */
static void
catch_sig(int sig)
{
	signal(sig,SIG_DFL);
	setuid(getuid());
	kill(getpid(),sig);
}

/*  Quit with the status after first removing our entry from the utmp file.
 */
void
quit(int status)
{
    exit(status);
}

/*  Run the command in a subprocess and return a file descriptor for the
 *  master end of the pseudo-teletype pair with the command talking to
 *  the slave.
 */
int
run_command(char *command, char **argv)
{
        int ptyfd, ttyfd;
        int i;

        for (i = 1; i <= 15; i++)
                signal(i, catch_sig);
        signal(SIGCHLD, catch_child);
	fd_width = sysconf(_SC_OPEN_MAX);
        openpty(&ptyfd, &ttyfd, ttynam, NULL, NULL);
        comm_pid = fork();
        if (comm_pid < 0) {
                error("Can't fork");
                return (-1);
        }
        if (comm_pid == 0) {
                close(ptyfd);
                login_tty(ttyfd);
                execvp(command, argv);
                error("Couldn't execute %s", command);
                quit(1);
        }
        close(ttyfd);
        return (ptyfd);
}

/*  Tell the teletype handler what size the window is.  Called initially from
 *  the child and after a window size change from the parent.
 */
void
tty_set_size(int width, int height)
{
#ifdef TIOCSWINSZ
	struct winsize wsize;

	if (comm_pid < 0)
		return;
	wsize.ws_row = height;
	wsize.ws_col = width;
	wsize.ws_xpixel = 0;
	wsize.ws_ypixel = 0;
	ioctl((comm_pid == 0) ? 0 : comm_fd,TIOCSWINSZ,(char *)&wsize);
#endif /* TIOCSWINSZ */
}
