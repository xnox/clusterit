/* $Id: jsd.c,v 1.2 2000/01/14 23:29:32 garbled Exp $ */
/*
 * Copyright (c) 1998
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

/* Semi-intelligent job scheduling daemon.  Intended for heterogenous
 * network load sharing applications.
 *
 *
 * Note for the casual observer.  While this program does indeed share a
 * large number of routines with dsh, and common.c, it is *vital* that it
 * remain totally separate.  There are some minor differences in the routines
 * and they are not compatible.
 */


#if !defined(lint) && defined(__NetBSD__)
__COPYRIGHT(
"@(#) Copyright (c) 1998\n\
        Tim Rightnour.  All rights reserved\n");
#endif /* not lint */

#if !defined(lint) && defined(__NetBSD__)
__RCSID("$Id: jsd.c,v 1.2 2000/01/14 23:29:32 garbled Exp $");
#endif

extern int errno;
#ifndef __P
#define __P(protos) protos
#endif

/* globals */
int debug, errorflag, exclusion, sharedmem;
char *grouplist[MAX_CLUSTER];
float nodespeed[MAX_CLUSTER];
int freelist[MAX_CLUSTER];
char *rungroup[MAX_GROUPS];
pid_t currentchild;

/*
 * Main should handle deciding which group or nodes we run on, running the
 * initial benchmark run, and finally launching the scheduler daemon.
 */

int
main(argc, argv) 
	int argc;
	char *argv[];
{
	extern char	*optarg;
	extern int	optind;

	int someflag, ch, i, fanout, showflag, fanflag, portnum;
	char *p, *group, *nodelist[MAX_CLUSTER];
	char *nodename, *username, *exclude[MAX_CLUSTER], *benchcmd;
	struct rlimit	limit;
	pid_t pid;

	extern int debug, gotsigterm, gotsigint;

	someflag = showflag = fanflag = 0;
	exclusion = debug = 0;
	sharedmem = 1;
	gotsigint = gotsigterm = 0;
	fanout = DEFAULT_FANOUT;
	nodename = NULL;
	username = NULL;
	group = NULL;
	for (i=0; i < MAX_GROUPS; i++)
		rungroup[i] = NULL;

	while ((ch = getopt(argc, argv, "?diqf:g:l:w:x:p:")) != -1)
		switch (ch) {
		case 'd':		/* we want to debug jsd (hidden)*/
			debug = 1;
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
			for (p = optarg; p != NULL && i < MAX_GROUPS - 1; ) {
				group = (char *)strsep(&p, ",");
				if (group != NULL)
					rungroup[i++] = strdup(group);
			}
			rungroup[i] = '\0';
			group = NULL;
			break;			
		case 'x':		/* exclude nodes, w overrides this */
			exclusion = 1;
			i = 0;
			for (p = optarg; p != NULL && i < MAX_CLUSTER - 1; ) {
				nodename = (char *)strsep(&p, ",");
				if (nodename != NULL)
					exclude[i++] = strdup(nodename);
			}
			exclude[i] = '\0';
			break;
		case 'w':		/* perform operation on these nodes */
			someflag = 1;
			i = 0;
			for (p = optarg; p != NULL && i < MAX_CLUSTER - 1; ) {
				nodename = (char *)strsep(&p, ",");
				if (nodename != NULL)
					nodelist[i++] = strdup(nodename);
			}
			nodelist[i] = '\0';
			break;
		case 'p':		/* port to listen on, shuts off SHM */
			sharedmem = 0;
			portnum = atoi(optarg);
			break;
		case '?':		/* you blew it */
			(void)fprintf(stderr,
			    "usage: jsd [-iq] [-p port] [-f fanout] "
				"[-g rungroup1,...,rungroupN] "
			    "[-l username] [-x node1,...,nodeN] [-w node1,..,nodeN] "
			    "[command ...]\n");
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
		parse_cluster(nodename, exclude, nodelist);

	argc -= optind;
	argv += optind;
	if (showflag) {
		do_showcluster(nodelist, fanout);
		return(EXIT_SUCCESS);
	}
	openlog("jsd", LOG_PID|LOG_NDELAY, LOG_DAEMON);

	/*
	 * set per-process limits for max descriptors, this avoids running
	 * out of descriptors and the odd errors that come with that.
	 */
	pid = fork();
	if (pid == 0) {
		syslog(LOG_INFO, "Job Scheduling Daemon Started");
		if (getrlimit(RLIMIT_NOFILE, &limit) != 0)
			bailout(__LINE__);
		limit.rlim_cur = fanout * 5;
		if (setrlimit(RLIMIT_NOFILE, &limit) != 0)
			bailout(__LINE__);
		
		if (getenv("JSD_BENCH_CMD")) {
			benchcmd = getenv("JSD_BENCH_CMD")
				do_command(benchcmd, nodelist, fanout, username);
		} else {
			for (i=0; i < MAX_NODES; i++) {
				nodespeed[i] = 1.0;
				freelist[i] = 1;
			}
			syslog(LOG_WARNING, "No JSD_BENCH_CMD environment setting,"
				" assuming homogenus cluster.");
		}
	} else if (pid > 0)
		return(EXIT_SUCCESS);
	else if (pid == -1)
		syslog(LOG_CRIT, "Aborting: %s", strerror(errno));
	return(EXIT_FAILURE);
}

/* 
 * Simple error handling routine, needs severe work.
 * Its almost totally useless.
 */

/*ARGSUSED*/
void
bailout(line) 
	int line;
{
	extern int errno;
	
	if (debug)
		syslog(LOG_CRIT, "Failed on line %d: %s %d\n", line,
			strerror(errno), errno);
	else
		syslog(LOG_CRIT, "Internal error, aborting: %s\n",
			strerror(errno));

	_exit(EXIT_FAILURE);
}
