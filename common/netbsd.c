/* $Id: netbsd.c,v 1.5 2007/07/02 17:30:27 garbled Exp $ */

/*	$NetBSD: strsep.c,v 1.7 1998/02/03 18:49:23 perry Exp $	*/
/*	$NetBSD: pty.c,v 1.16 2000/07/10 11:16:38 ad Exp $	*/
/*	$NetBSD: login_tty.c,v 1.10 2000/07/05 11:46:41 ad Exp $	*/
/*      $NetBSD: humanize_number.c,v 1.12 2007/03/13 02:52:10 enami Exp $ */

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1997, 1998, 1999, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center, by Luke Mewburn and by Tomas Svensson.
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
 *      This product includes software developed by the NetBSD
 *      Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include "config.h"

#include <string.h>
#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/syslog.h>

#include <signal.h>
#include <assert.h>
#include <inttypes.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <locale.h>

#include "../common/common.h"

#ifdef __SVR4
int
revoke(path)
        const char *path;
{
        vhangup();        /* XXX */
        return 0;
}
#endif

#define TTY_LETTERS	"pqrstuvwxyzPQRST"

#ifndef HAVE_STRSEP

/*
 * Get next token from string *stringp, where tokens are possibly-empty
 * strings separated by characters from delim.  
 *
 * Writes NULs into the string at *stringp to end tokens.
 * delim need not remain constant from call to call.
 * On return, *stringp points past the last NUL written (if there might
 * be further tokens), or is NULL (if there are definitely no more tokens).
 *
 * If *stringp is NULL, strsep returns NULL.
 */
char *
strsep(stringp, delim)
	char **stringp;
	const char *delim;
{
	char *s;
	const char *spanp;
	int c, sc;
	char *tok;

	if ((s = *stringp) == NULL)
		return (NULL);
	for (tok = s;;) {
		c = *s++;
		spanp = delim;
		do {
			if ((sc = *spanp++) == c) {
				if (c == 0)
					s = NULL;
				else
					s[-1] = 0;
				*stringp = s;
				return (tok);
			}
		} while (sc != 0);
	}
	/* NOTREACHED */
}

#endif /* HAVE_STRSEP */

#ifndef HAVE_OPENPTY

int
openpty(int *amaster, int *aslave, char *name, struct termios *termp, 
	struct winsize *winp)
{
	static char line[] = "/dev/XtyXX";
	const char *cp1, *cp2;
	int master, slave;
	gid_t ttygid;
	struct group *gr;

	assert(amaster != NULL);
	assert(aslave != NULL);
	/* name may be NULL */
	/* termp may be NULL */
	/* winp may be NULL */

	if ((gr = getgrnam("tty")) != NULL)
		ttygid = gr->gr_gid;
	else
		ttygid = (gid_t) -1;

	for (cp1 = TTY_LETTERS; *cp1; cp1++) {
		line[8] = *cp1;
		for (cp2 = "0123456789abcdef"; *cp2; cp2++) {
			line[5] = 'p';
			line[9] = *cp2;
			if ((master = open(line, O_RDWR, 0)) == -1) {
				if (errno == ENOENT)
					return (-1);	/* out of ptys */
			} else {
				line[5] = 't';
				(void) chown(line, getuid(), ttygid);
				(void) chmod(line, S_IRUSR|S_IWUSR|S_IWGRP);
				(void) revoke(line);
				if ((slave = open(line, O_RDWR, 0)) != -1) {
					*amaster = master;
					*aslave = slave;
					if (name)
						strcpy(name, line);
					if (termp)
						(void) tcsetattr(slave,
							TCSAFLUSH, termp);
					if (winp)
						(void) ioctl(slave, TIOCSWINSZ,
						    winp);
					return (0);
				}
				(void) close(master);
			}
		}
	}
	errno = ENOENT;	/* out of ptys */
	return (-1);
}

#endif /* HAVE_OPENPTY */

#ifndef HAVE_LOGIN_TTY

int
login_tty(int fd)
{
	int mypgrp = getpid();
	void (*old)(int);

	if (setpgid(0, mypgrp) == -1) {
		syslog(LOG_AUTH|LOG_NOTICE, "setpgrp(0, %d): %m", mypgrp);
		return(-1);
	}
	old = signal(SIGTTOU, SIG_IGN);
	if (ioctl(fd, TIOCSPGRP, &mypgrp) == -1) {
		syslog(LOG_AUTH|LOG_NOTICE, "ioctl(%d, TIOCSPGRP, %d): %m",
		    fd, mypgrp);
		return(-1);
	}
	(void) signal(SIGTTOU, old);

	(void) dup2(fd, 0);
	(void) dup2(fd, 1);
	(void) dup2(fd, 2);
	if (fd > STDERR_FILENO)
		(void) close(fd);
	return (0);
}
#endif /* HAVE_LOGIN_TTY */

#ifndef HAVE_HUMANIZE_NUMBER

int
humanize_number(char *buf, size_t len, int64_t bytes,
    const char *suffix, int scale, int flags)
{
	const char *prefixes, *sep;
	int	b, i, r, maxscale, s1, s2, sign;
	int64_t	divisor, max;
	size_t	baselen;

	assert(buf != NULL);
	assert(suffix != NULL);
	assert(scale >= 0);

	if (flags & HN_DIVISOR_1000) {
		/* SI for decimal multiplies */
		divisor = 1000;
		if (flags & HN_B)
			prefixes = "B\0k\0M\0G\0T\0P\0E";
		else
			prefixes = "\0\0k\0M\0G\0T\0P\0E";
	} else {
		/*
		 * binary multiplies
		 * XXX IEC 60027-2 recommends Ki, Mi, Gi...
		 */
		divisor = 1024;
		if (flags & HN_B)
			prefixes = "B\0K\0M\0G\0T\0P\0E";
		else
			prefixes = "\0\0K\0M\0G\0T\0P\0E";
	}

#define	SCALE2PREFIX(scale)	(&prefixes[(scale) << 1])
	maxscale = 7;

	if (scale >= maxscale &&
	    (scale & (HN_AUTOSCALE | HN_GETSCALE)) == 0)
		return (-1);

	if (buf == NULL || suffix == NULL)
		return (-1);

	if (len > 0)
		buf[0] = '\0';
	if (bytes < 0) {
		sign = -1;
		bytes *= -100;
		baselen = 3;		/* sign, digit, prefix */
	} else {
		sign = 1;
		bytes *= 100;
		baselen = 2;		/* digit, prefix */
	}
	if (flags & HN_NOSPACE)
		sep = "";
	else {
		sep = " ";
		baselen++;
	}
	baselen += strlen(suffix);

	/* Check if enough room for `x y' + suffix + `\0' */
	if (len < baselen + 1)
		return (-1);

	if (scale & (HN_AUTOSCALE | HN_GETSCALE)) {
		/* See if there is additional columns can be used. */
		for (max = 100, i = len - baselen; i-- > 0;)
			max *= 10;

		/*
		 * Divide the number until it fits the given column.
		 * If there will be an overflow by the rounding below,
		 * divide once more.
		 */
		for (i = 0; bytes >= max - 50 && i < maxscale; i++)
			bytes /= divisor;

		if (scale & HN_GETSCALE)
			return (i);
	} else
		for (i = 0; i < scale && i < maxscale; i++)
			bytes /= divisor;

	/* If a value <= 9.9 after rounding and ... */
	if (bytes < 995 && i > 0 && flags & HN_DECIMAL) {
		/* baselen + \0 + .N */
		if (len < baselen + 1 + 2)
			return (-1);
		b = ((int)bytes + 5) / 10;
		s1 = b / 10;
		s2 = b % 10;
		r = snprintf(buf, len, "%d%s%d%s%s%s",
		    sign * s1, localeconv()->decimal_point, s2,
		    sep, SCALE2PREFIX(i), suffix);
	} else
		r = snprintf(buf, len, "%" PRId64 "%s%s%s",
		    sign * ((bytes + 50) / 100),
		    sep, SCALE2PREFIX(i), suffix);

	return (r);
}
#endif /* HAVE_HUMANIZE_NUMBER */
