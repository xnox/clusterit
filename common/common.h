/* $Id: common.h,v 1.15 2007/01/22 18:48:16 garbled Exp $ */
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

/* Headers for common.c, used by dsh-like programs. */
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif /* HAVE_TERMIOS_H */
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>
#include <sys/time.h>

#ifdef HAVE_UTIL_H
#include <util.h>
#endif /* HAVE_UTIL_H */

#ifdef USE_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#include <X11/cursorfont.h>
#endif

enum {
    DEFAULT_FANOUT = 64,
    GROUP_MALLOC = 16,
    MAXBUF = 1024
};

typedef struct {
    int fds[2];
} pipe_t;

typedef struct node_data {
    char *name;			/* node name */
    int group;			/* member of this group */
    pipe_t err, out;		/* pipe structures */
    pid_t childpid;		/* pid of the child */
    struct node_data *next;	/* pointer to next node */
    int free;
    double index;
#ifdef USE_X11
    Window win_id;		/* the window ID of node's window */
#endif
} node_t;

typedef struct group_data {
    char *name;		/* group name */
    int numlump;	/* number of lumps I am a member of */
    int *lump;		/* member of this lump */
} group_t;

void _bailout(int line, char *file);

#define bailout() _bailout(__LINE__, __FILE__)

#ifndef HAVE_STRSEP
char *strsep(char **stringp, const char *delim);
#endif
#ifndef HAVE_LOGIN_TTY
int login_tty(int fd);
#endif
#ifndef HAVE_OPENPTY
int openpty(int *amaster, int *aslave, char *name, struct termios *termp,
    struct winsize *winp);
#endif

#ifdef CLUSTERS
void do_showcluster(int fanout);
int parse_cluster(char **exclude);
node_t *nodealloc(char *nodename);
int test_node_connection(int rshport, int timeout, node_t *nodeptr);
char **parse_rcmd(char *rcmd_env, char *args_env, int *nrofargs);
char *default_rcmd(char *rcmd_env);
int get_rshport(int testflag, int rshport, char *rcmd_env);
char *build_rshstring(char **rsh, int nrofargs);
int parse_gopt(char *oa);
char **parse_xopt(char *oa);

extern char **lumplist;
extern char **rungroup;
extern int exclusion, debug, grouping, nrofrungroups;
extern volatile sig_atomic_t alarmtime;
extern group_t *grouplist;
extern node_t *nodelink;
#endif /* CLUSTERS */

extern int debug;
extern char *progname;

#if defined(DEFAULT_RSH)
#ifndef RCMD_DEFAULT
#define RCMD_DEFAULT "rsh"
#endif
#ifndef RLOGIN_DEFAULT
#define RLOGIN_DEFAULT "rsh"
#endif
#ifndef RCP_DEFAULT
#define RCP_DEFAULT "rcp"
#endif
#ifndef RVT_DEFAULT
#define RVT_DEFAULT "rvt"
#endif
#ifndef TEST_PORT
#define TEST_PORT 514
#endif
#else /* use ssh */
#ifndef RCMD_DEFAULT
#define RCMD_DEFAULT "ssh"
#endif
#ifndef RLOGIN_DEFAULT
#define RLOGIN_DEFAULT "ssh"
#endif
#ifndef RCP_DEFAULT
#define RCP_DEFAULT "scp"
#endif
#ifndef RVT_DEFAULT
#define RVT_DEFAULT "rvt"
#endif
#ifndef TEST_PORT
#define TEST_PORT 22
#endif



#endif /* USE_SSH */
