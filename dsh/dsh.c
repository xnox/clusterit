/* $Id: dsh.c,v 1.9 1999/05/04 19:20:43 garbled Exp $ */
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

#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#if !defined(lint) && defined(__NetBSD__)
__COPYRIGHT(
"@(#) Copyright (c) 1998\n\
        Tim Rightnour.  All rights reserved\n");
#endif /* not lint */

#if !defined(lint) && defined(__NetBSD__)
__RCSID("$Id: dsh.c,v 1.9 1999/05/04 19:20:43 garbled Exp $");
#endif

enum {
	MAX_CLUSTER = 512,
	DEFAULT_FANOUT = 64,
	MAX_GROUPS = 32,
	MAXBUF = 1024
};

extern int errno;
#ifndef __P
#define __P(protos) protos
#endif

void bailout __P((int));
void do_command __P((char **, char *[], int, char *));
void do_showcluster __P((char *[], int));
int test_node __P((int));
char *alignstring __P((char *, size_t));
void sig_handler __P((int));

#ifndef __NetBSD__
char * strsep(char **stringp, const char *delim);
#endif

int debug, errorflag, gotsigint, gotsigterm;
char *grouplist[MAX_CLUSTER];
char *rungroup[MAX_GROUPS];
pid_t currentchild;

/* 
 *  dsh is a cluster management tool derrived from the IBM tool of the
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

	FILE	*fd;
	int someflag, ch, i, fanout, showflag, exclusion, j, fail, fanflag;
	char *p, *group, *nodelist[MAX_CLUSTER];
	char *nodename, *clusterfile, *username, *exclude[MAX_CLUSTER];
	char	buf[256];
	struct rlimit	limit;

	extern int debug, errorflag, gotsigterm, gotsigint;

	someflag = 0;
	showflag = 0;
	fanflag = 0;
	exclusion = 0;
	debug = 0;
	errorflag = 0;
	gotsigint = 0;
	gotsigterm = 0;
	fanout = DEFAULT_FANOUT;
	username = NULL;
	group = NULL;
	for (i=0; i < MAX_GROUPS; i++)
		rungroup[i] = NULL;

	while ((ch = getopt(argc, argv, "?eiqf:g:l:w:x:")) != -1)
		switch (ch) {
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
		case '?':		/* you blew it */
			(void)fprintf(stderr,
			    "usage: dsh [-eiq] [-f fanout] [-g rungroup1,...,rungroupN] "
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
	if (!someflag) {
/* if -w wasn't specified, we need to parse the cluster file */
		clusterfile = getenv("CLUSTER");
		if (clusterfile == NULL) {
			(void)fprintf(stderr,
			    "must use -w flag without CLUSTER environment setting.\n");
			return(EXIT_FAILURE);
		}
		fd = fopen(clusterfile, "r");
		i = 0;
		while ((nodename = fgets(buf, sizeof(buf), fd))) {
			p = (char *)strsep(&nodename, "\n");
			if (strcmp(p, "") != 0) {
				if (exclusion) {		/* this handles the -x option */
					fail = 0;
					for (j = 0; exclude[j] != NULL; j++)
						if (strcmp(p, exclude[j]) == 0)
							fail = 1;
					if (!fail) {
						if (strstr(p, "GROUP") != NULL) {
							strsep(&p, ":");
							group = strdup(p);
						} else {
							if (group == NULL)
								grouplist[i] = NULL;
							else
								grouplist[i] = (char *)strdup(group);
							nodelist[i++] = (char *)strdup(p);
						}
					}
				} else {
					if (strstr(p, "GROUP") != NULL) {
						strsep(&p, ":");
						group = strdup(p);
					} else {
						if (group == NULL)
							grouplist[i] = NULL;
						else
							grouplist[i] = (char *)strdup(group);
						nodelist[i++] = (char *)strdup(p);
					}
				} /* exlusion */
			} /* strcmp */
		} /* while nodename */
		nodelist[i] = '\0';
		grouplist[i] = '\0';
		fclose(fd);
	}
	argc -= optind;
	argv += optind;
	if (showflag) {
		do_showcluster(nodelist, fanout);
		return(EXIT_SUCCESS);
	}

	/*
	 * set per-process limits for max descriptors, this avoids running
	 * out of descriptors and the odd errors that come with that.
	 */
	if (getrlimit(RLIMIT_NOFILE, &limit) != 0)
		bailout(__LINE__);
	limit.rlim_cur = fanout * 5;
	if (setrlimit(RLIMIT_NOFILE, &limit) != 0)
		bailout(__LINE__);

	do_command(argv, nodelist, fanout, username);
	return(EXIT_SUCCESS);
}

/*
 * This routine just rips open the various arrays and prints out information
 * about what the command would have done, and the topology of your cluster.
 * Invoked via the -q switch.
 */

void
do_showcluster(nodelist, fanout)
	char *nodelist[];
	int fanout;
{
	int i, j, l, n;

	i = l = 0;
	for (i=0; nodelist[i] != NULL; i++)		/* just count the nodes */
		;
	j = i / fanout;
      /* how many times do I have to run in order to reach them all */
	if (i % fanout)
		j++;

	if (rungroup[0] != NULL) {
		(void)printf("Rungroup:");
		for (i=0; rungroup[i] != NULL; i++) {
			if (!(i % 4) && i > 0)
				(void)printf("\n");
			(void)printf("\t%s", rungroup[i]);
		}
		if (i % 4)
			(void)printf("\n");
	}	

	if (getenv("CLUSTER"))
		(void)printf("Cluster file: %s\n", getenv("CLUSTER"));
	(void)printf("Fanout size: %d\n", fanout);
	for (n=0; n <= j; n++) {
		for (i=n * fanout;
		    ((nodelist[i] != NULL) && (i < (n + 1) * fanout)); i++) {
			if (rungroup[0] != NULL) {
				if (test_node(i)) {
					l++;
					if (grouplist[i] == NULL)
						(void)printf("Node: %3d  Fangroup: %3d  Rungroup: None"
						    "             Host: %s\n", l, n + 1, nodelist[i]);
					else
						(void)printf("Node: %3d  Fangroup: %3d  Rungroup: "
						    "%-15s  Host: %-15s\n", l, n + 1, grouplist[i],
							nodelist[i]);
				}
			} else {
				l++;
				if (grouplist[i] == NULL)
					(void)printf("Node: %3d  Fangroup: %3d  Rungroup: None"
					    "            Host: %-15s\n", l, n + 1, nodelist[i]);
				else
					(void)printf("Node: %3d  Fangroup: %3d  Rungroup: %-15s"
					    "  Host: %-15s\n", l, n + 1, grouplist[i],
						nodelist[i]);
			}
		}
	}
}

/* 
 * Do the actual dirty work of the program, now that the arguments
 * have all been parsed out.
 */

void
do_command(argv, nodelist, fanout, username)
	char **argv;
	char *nodelist[];
	char *username;
	int fanout;
{
	typedef struct { int fds[2]; } pipe_t;
	struct sigaction signaler;

	FILE *fd, *in;
	pipe_t *out;
	pipe_t *err;
	pid_t *childpid;
	char buf[MAXBUF];
	char pipebuf[2048];
	int status, i, j, n, g, piping;
	size_t maxnodelen;
	char *p, *command, *rsh, *cd;

	extern int debug, gotsigterm, gotsigint;

	out = malloc((fanout+1)*sizeof(pipe_t));
	err = malloc((fanout+1)*sizeof(pipe_t));
	childpid = malloc((fanout+1)*sizeof(pid_t));

	if (err == NULL || out == NULL || childpid == NULL) {
		fprintf(stderr, "Out of memory");
		exit(EXIT_FAILURE);
	}

	maxnodelen = 0;
	j = i = 0;
	piping = 0;
	in = NULL;
	cd = pipebuf;

	if (debug) {
		if (username != NULL)
			(void)printf("As User: %s\n", username);
		(void)printf("On nodes:");
		for (i=0; nodelist[i] != NULL; i++) {
			if (!(j % 4) && j > 0)
				(void)printf("\n");
			if (rungroup[0] != NULL) {
				if (test_node(i)) {
					(void)printf("%s\t", nodelist[i]);
					j++;
				}
			} else {
				(void)printf("%s\t", nodelist[i]);
				j++;
			}
		}
	}
	for (i=0; nodelist[i] != NULL; i++)		/* just count the nodes */
		if (strlen(nodelist[i]) > maxnodelen)
			maxnodelen = strlen(nodelist[i]); /* compute max hostname len */
	
	j = i / fanout;
	if (i % fanout)
		j++;

	/* construct the command from the remains of argv */
	command = (char *)malloc(MAXBUF * sizeof(char));
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
			(void)printf("dsh>");
		in = fdopen(STDIN_FILENO, "r");
		/* start reading stuff from stdin and process */
		command = fgets(buf, sizeof(buf), in);
		if (command != NULL)
			if (strcmp(command,"\n") == 0)
				command = NULL;
	}

	signaler.sa_handler = sig_handler;
	signaler.sa_flags |= SA_RESTART;
	sigaction(SIGTERM, &signaler, NULL);
	sigaction(SIGINT, &signaler, NULL);

	while (command != NULL) {
		for (n=0; n <= j; n++) {
			if (gotsigterm || gotsigint)
				exit(EXIT_FAILURE);
			for (i=n * fanout;
			    ((nodelist[i] != NULL) && (i < (n + 1) * fanout)); i++) {
				if (gotsigterm)
					exit(EXIT_FAILURE);
				if (test_node(i)) {
					g = i - n * fanout;
#ifdef DEBUG
					(void)printf("Working node: %d, group %d, fanout part: "
					    "%d\n", i, n, g);
#endif
/*
 * we set up pipes for each node, to prepare for the oncoming barrage of data.
 * Close on exec must be set here, otherwise children spawned after other
 * children, inherit the open file descriptors, and cause the pipes to remain
 * open forever.
 */
					if (pipe(out[g].fds) != 0)
						bailout(__LINE__);
					if (pipe(err[g].fds) != 0)
						bailout(__LINE__);
					if (fcntl(out[g].fds[0], F_SETFD, 1) == -1)
						bailout(__LINE__);
					if (fcntl(out[g].fds[1], F_SETFD, 1) == -1)
						bailout(__LINE__);
					if (fcntl(err[g].fds[0], F_SETFD, 1) == -1)
						bailout(__LINE__);
					if (fcntl(err[g].fds[1], F_SETFD, 1) == -1)
						bailout(__LINE__);
					childpid[g] = fork();
					switch (childpid[g]) {
					  /* its the ol fork and switch routine eh? */
						case -1:
							bailout(__LINE__);
							break;
						case 0:
							/* remove from parent group to avoid kernel
							 * passing signals to children.
							 */
							(void)setsid();
							if (dup2(out[g].fds[1], STDOUT_FILENO)
							    != STDOUT_FILENO) 
								bailout(__LINE__);
							if (dup2(err[g].fds[1], STDERR_FILENO)
							    != STDERR_FILENO)
								bailout(__LINE__);
							if (close(out[g].fds[0]) != 0)
								bailout(__LINE__);
							if (close(err[g].fds[0]) != 0)
								bailout(__LINE__);
							rsh = getenv("RCMD_CMD");
							if (rsh == NULL)
								rsh = "rsh";
#ifdef DEBUG
							(void)printf("%s %s %s\n", rsh, nodelist[i],
							    command);
#endif
							if (username != NULL)
/* interestingly enough, this -l thing works great with ssh */
								execlp(rsh, rsh, "-l", username, nodelist[i],
								    command, (char *)0);
							else
								execlp(rsh, rsh, nodelist[i], command,
								    (char *)0);
							bailout(__LINE__);
						default:
							break;
						} /* switch */
				} /* test_node */
			} /* for i */
			for (i=n * fanout;
			    ((nodelist[i] != NULL) && (i < (n + 1) * fanout)); i++) {
				if (test_node(i)) {
					g = i - n * fanout;
					if (gotsigterm)
						exit(EXIT_FAILURE);
					currentchild = childpid[g];
					/* now close off the useless stuff, and read the goodies */
					if (close(out[g].fds[1]) != 0)
						bailout(__LINE__);
					if (close(err[g].fds[1]) != 0)
						bailout(__LINE__);
					fd = fdopen(out[g].fds[0], "r"); /* stdout */
					if (fd == NULL)
						bailout(__LINE__);
					while ((cd = fgets(pipebuf, sizeof(pipebuf), fd))) {
						if (cd != NULL)
							(void)printf("%s: %s",
							    alignstring(nodelist[i], maxnodelen), cd);
					}
					fclose(fd);
					fd = fdopen(err[g].fds[0], "r"); /* stderr */
					if (fd == NULL)
						bailout(__LINE__);
					while ((cd = fgets(pipebuf, sizeof(pipebuf), fd))) {
						if (errorflag && cd != NULL)
							(void)printf("%s: %s",
								alignstring(nodelist[i], maxnodelen), cd);
					}
					fclose(fd);
					(void)wait(&status);
				} /* test_node */
			} /* for pipe read */
		} /* for n */
		if (piping) {
/* yes, this is code repetition, no need to adjust your monitor */
			if (isatty(STDIN_FILENO) && piping)
				(void)printf("dsh>");
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

/* test routine, saves a ton of repetive code */

int
test_node(count)
	int count;
{
	int i;

	if (rungroup[0] == NULL)
		return(1);
	else
		if (grouplist[count] != NULL)
			for (i=0; rungroup[i] != NULL; i++)
				if (strcmp(rungroup[i], grouplist[count]) == 0)
					return(1);
	return(0);
}

/* return a string, followed by n - strlen spaces */

char *
alignstring(string, n)
	char *string;
	size_t n;
{
	size_t i;
	char *newstring;

	newstring = strdup(string);
	for (i=1; i <= n - strlen(string); i++)
		newstring = strcat(newstring, " ");

	return(newstring);
}


/* 
 * Simple error handling routine, needs severe work.
 * Its almost totally useless.
 */

void
bailout(line) 
	int line;
{
/*ARGSUSED*/
extern int errno;

#ifdef DEBUG
	(void)fprintf(stderr, "Failed on line %d: %s %d\n", line, strerror(errno), errno);
#else
	(void)fprintf(stderr, "Internal error, aborting: %s\n", strerror(errno));
#endif
	_exit(EXIT_FAILURE);
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
			(void)fprintf(stderr, "Recieved unknown signal in sig_handler\n");
			_exit(EXIT_FAILURE);
			break;
	}
}
