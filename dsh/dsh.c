/* $Id: dsh.c,v 1.16 2000/02/17 07:59:23 garbled Exp $ */
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

#include <errno.h>
#include <fcntl.h>
#include <signal.h>

#include "../common/common.h"

#if !defined(lint) && defined(__NetBSD__)
__COPYRIGHT(
"@(#) Copyright (c) 1998, 1999, 2000\n\
        Tim Rightnour.  All rights reserved\n");
__RCSID("$Id: dsh.c,v 1.16 2000/02/17 07:59:23 garbled Exp $");
#endif /* not lint */

extern int errno;
#ifndef __P
#define __P(protos) protos
#endif

void do_command __P((char **, int, char *));
void sig_handler __P((int));

/* globals */
int debug, errorflag, gotsigint, gotsigterm, exclusion, grouping;
node_t *nodelink;
group_t *grouplist;
char **rungroup;
char **lumplist;
pid_t currentchild;
char *progname;

/* 
 *  dsh is a cluster management tool based upon the IBM tool of the
 *  same name.  It allows a user, or system administrator to issue
 *  commands in paralell on a group of machines.
 */

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char	*optarg;
	extern int	optind;
	extern int debug, errorflag, gotsigterm, gotsigint;

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
		bailout(__LINE__);
	exclude = malloc(sizeof(char **) * GROUP_MALLOC);
	if (exclude == NULL)
		bailout(__LINE__);

	progname = p = q = argv[0];
	while (progname != NULL) {
		q = progname;
		progname = (char *)strsep(&p, "/");
	}
	progname = strdup(q);

	while ((ch = getopt(argc, argv, "?deiqf:g:l:w:x:")) != -1)
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
							bailout(__LINE__);
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
							bailout(__LINE__);
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
			    "[-l username] [-x node1,...,nodeN] [-w node1,..,nodeN] "
			    "[command ...]\n", progname);
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
		bailout(__LINE__);
	if (limit.rlim_cur < fanout * 5) {
		limit.rlim_cur = fanout * 5;
		if (setrlimit(RLIMIT_NOFILE, &limit) != 0)
			bailout(__LINE__);
	}

	do_command(argv, fanout, username);
	return(EXIT_SUCCESS);
}



/* 
 * Do the actual dirty work of the program, now that the arguments
 * have all been parsed out.
 */

void
do_command(argv, fanout, username)
	char **argv;
	char *username;
	int fanout;
{
	struct sigaction signaler;

	FILE *fd, *in;
	char buf[MAXBUF];
	char pipebuf[2048];
	int status, i, j, n, g, piping;
	size_t maxnodelen;
	char *p, *command, *rsh, *cd;
	node_t *nodeptr, *nodehold;

	extern int debug, gotsigterm, gotsigint;

	maxnodelen = 0;
	j = i = 0;
	piping = 0;
	in = NULL;
	cd = pipebuf;

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

	/* construct the command from the remains of argv */
	command = (char *)malloc(MAXBUF * sizeof(char));
	memcpy(command, "\0", MAXBUF * sizeof(char));
	for (p = *argv; p != NULL; p = *++argv ) {
		strcat(command, p);
		strcat(command, " ");
	}
	if (debug) {
		(void)printf("\nDo Command: %s\n", command);
		(void)printf("Fanout: %d Groups:%d\n", fanout, j);
	}
	if (strcmp(command,"") == 0) {
		piping = 1;
		/* are we a terminal?  then go interactive! */
		if (isatty(STDIN_FILENO) && piping)
			(void)printf("%s>", progname);
		in = fdopen(STDIN_FILENO, "r");
		/* start reading stuff from stdin and process */
		command = fgets(buf, sizeof(buf), in);
		if (command != NULL)
			if (strcmp(command,"\n") == 0)
				command = NULL;
	} else {
		close(STDIN_FILENO); /* DAMN this bug took awhile to find */
	}

	signaler.sa_handler = sig_handler;
	signaler.sa_flags |= SA_RESTART;
	sigaction(SIGTERM, &signaler, NULL);
	sigaction(SIGINT, &signaler, NULL);
	g = 0;
	nodeptr = nodelink;
	while (command != NULL) {
		for (n=0; n <= j; n++) {
			if (gotsigterm || gotsigint)
				exit(EXIT_FAILURE);
			nodehold = nodeptr;
			for (i=0; (i < fanout && nodeptr != NULL); i++) {
				g++;
				if (gotsigterm)
					exit(EXIT_FAILURE);
				if (debug)
					(void)printf("Working node: %d, fangroup %d,"
						" fanout part: %d\n", g, n, i);
/*
 * we set up pipes for each node, to prepare for the oncoming barrage of data.
 * Close on exec must be set here, otherwise children spawned after other
 * children, inherit the open file descriptors, and cause the pipes to remain
 * open forever.
 */
				if (pipe(nodeptr->out.fds) != 0)
					bailout(__LINE__);
				if (pipe(nodeptr->err.fds) != 0)
					bailout(__LINE__);
				if (fcntl(nodeptr->out.fds[0], F_SETFD, 1) == -1)
					bailout(__LINE__);
				if (fcntl(nodeptr->out.fds[1], F_SETFD, 1) == -1)
					bailout(__LINE__);
				if (fcntl(nodeptr->err.fds[0], F_SETFD, 1) == -1)
					bailout(__LINE__);
				if (fcntl(nodeptr->err.fds[1], F_SETFD, 1) == -1)
					bailout(__LINE__);
				nodeptr->childpid = fork();
				switch (nodeptr->childpid) {
					/* its the ol fork and switch routine eh? */
				case -1:
					bailout(__LINE__);
					break;
				case 0:
					/* remove from parent group to avoid kernel
					 * passing signals to children.
					 */
					(void)setsid();
					if (piping)
						if (close(STDIN_FILENO) != 0)
							bailout(__LINE__);
					if (dup2(nodeptr->out.fds[1], STDOUT_FILENO) 
						!= STDOUT_FILENO) 
						bailout(__LINE__);
					if (dup2(nodeptr->err.fds[1], STDERR_FILENO)
						!= STDERR_FILENO)
						bailout(__LINE__);
					if (close(nodeptr->out.fds[0]) != 0)
						bailout(__LINE__);
					if (close(nodeptr->err.fds[0]) != 0)
						bailout(__LINE__);
					rsh = getenv("RCMD_CMD");
					if (rsh == NULL)
						rsh = "rsh";
					if (debug)
						(void)printf("%s %s %s\n", rsh, nodeptr->name,
							command);
					if (username != NULL)
/* interestingly enough, this -l thing works great with ssh */
						execlp(rsh, rsh, "-l", username, nodeptr->name,
							command, (char *)0);
					else
						execlp(rsh, rsh, nodeptr->name, command, (char *)0);
					bailout(__LINE__);
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
					(void)printf("Printing node: %d, fangroup %d,"
						" fanout part: %d\n", g-fanout+i, n, i);
				currentchild = nodeptr->childpid;
				/* now close off the useless stuff, and read the goodies */
				if (close(nodeptr->out.fds[1]) != 0)
					bailout(__LINE__);
				if (close(nodeptr->err.fds[1]) != 0)
					bailout(__LINE__);
				fd = fdopen(nodeptr->out.fds[0], "r"); /* stdout */
				if (fd == NULL)
					bailout(__LINE__);
				while ((cd = fgets(pipebuf, sizeof(pipebuf), fd))) {
					if (cd != NULL)
						(void)printf("%s: %s",
							alignstring(nodeptr->name, maxnodelen), cd);
				}
				fclose(fd);
				fd = fdopen(nodeptr->err.fds[0], "r"); /* stderr */
				if (fd == NULL)
					bailout(__LINE__);
				while ((cd = fgets(pipebuf, sizeof(pipebuf), fd))) {
					if (errorflag && cd != NULL)
						(void)printf("%s: %s",
							alignstring(nodeptr->name, maxnodelen), cd);
				}
				fclose(fd);
				(void)wait(&status);
				nodeptr = nodeptr->next;
			} /* for pipe read */			
		} /* for n */
		if (piping) {
/* yes, this is code repetition, no need to adjust your monitor */
			if (isatty(STDIN_FILENO) && piping)
				(void)printf("%s>", progname);
			command = fgets(buf, sizeof(buf), in);
			if (command != NULL)
				if (strcmp(command,"\n") == 0)
					command = NULL;
		} else
			command = NULL;
	} /* while loop */
		if (piping) {  /* I learned this the hard way */
			fflush(in);
			fclose(in);
		}
}

void
sig_handler(i)
	int i;
{
	extern int gotsigterm;
	extern pid_t currentchild;

	switch (i) {
		case SIGINT:
			killpg(currentchild, SIGINT);
			break;
	    case SIGTERM:
			gotsigterm = 1;
			break;
		default:
			bailout(__LINE__);
			break;
	}
}
