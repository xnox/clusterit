/* $Id: run.c,v 1.1 1998/10/15 20:54:58 garbled Exp $ */
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

#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#ifndef lint
__COPYRIGHT(
"@(#) Copyright (c) 1998\n\
        Tim Rightnour.  All rights reserved\n");
#endif /* not lint */

#ifndef lint
__RCSID("$Id: run.c,v 1.1 1998/10/15 20:54:58 garbled Exp $");
#endif

#define MAX_CLUSTER 512
#define DEFAULT_FANOUT 64

extern int errno;
#ifdef __NetBSD__
void bailout __P((int));
void do_command __P((char **argv, char *nodelist[], int allflag, char *username));
void do_showcluster __P((char *nodelist[]));
int test_node __P((int count));
int check_rand __P((char *nodelist[]));
#else
void bailout(int);
void do_command(char **argv, char *nodelist[], int allflag, char *username);
char * strsep(char **stringp, const char *delim);
void do_showcluster(char *nodelist[]);
int test_node(int count);
int check_rand(char *nodelist[]);
#endif

int debug;
int errorflag;
char *grouplist[MAX_CLUSTER];
char *rungroup;

/* 
 *  seq is a cluster management tool derrived from the IBM tool of the
 *  same name.  It allows a user, or system administrator to issue
 *  commands in paralell on a group of machines.
 */

void main(argc, argv) 
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;

	FILE *fd;
	int someflag, ch, i, allflag, showflag, exclusion, j, fail;
	char *p, *group, *nodelist[MAX_CLUSTER], *nodename, *clusterfile, *username;
	char *exclude[MAX_CLUSTER];
	char buf[256];

	extern int debug;
	extern int errorflag;

	someflag = 0;
	showflag = 0;
	exclusion = 0;
	debug = 0;
	errorflag = 0;
	allflag = 0;
	username = NULL;
	group = NULL;
	rungroup = NULL;

	while ((ch = getopt(argc, argv, "?aeiqg:l:w:x:")) != -1)
		switch (ch) {
		case 'a':		/* set the allrun flag */
			allflag = 1;
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
		case 'g':		/* pick a group to run on */
			rungroup = strdup(optarg);
			break;			
		case 'x':		/* exclude nodes, w overrides this */
			exclusion = 1;
			i = 0;
			for (p = optarg; p != NULL; ) {
				nodename = (char *)strsep(&p, ",");
				if (nodename != NULL)
					exclude[i++] = strdup(nodename);
			}
			exclude[i] = '\0';
			break;
		case 'w':		/* perform operation on these nodes */
			someflag = 1;
			i = 0;
			for (p = optarg; p != NULL; ) {
				nodename = (char *)strsep(&p, ",");
				if (nodename != NULL)
					nodelist[i++] = strdup(nodename);
			}
			nodelist[i] = '\0';
			break;
		case '?':		/* you blew it */
			(void)fprintf(stderr,
			    "usage: seq [-aeiq] [-g rungroup] [-l username] [-x node1,...,nodeN] [-w node1,..,nodeN] [command ...]\n");
			exit(EXIT_FAILURE);
			break;
		default:
			break;
	}
	if (!someflag) { /* if -w wasn't specified, we need to parse the cluster file */
		clusterfile = getenv("CLUSTER");
		if (clusterfile == NULL) {
			fprintf(stderr, "must use -w flag without CLUSTER environment setting.\n");
			exit(EXIT_FAILURE);
		}
		fd = fopen(clusterfile, "r");
		i = 0;
		while ((nodename = fgets(buf, sizeof(buf), fd))) {
			p = (char *)strsep(&nodename, "\n");
			if (strcmp(p, "") != 0) 
				if (exclusion) {		/* this handles the -x option */
					fail = 0;
					for (j = 0; exclude[j] != NULL; j++)
						if (strcmp(p,exclude[j]) == 0)
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
				}
		}
		nodelist[i] = '\0';
		grouplist[i] = '\0';
		fclose(fd);
	}
	argc -= optind;
	argv += optind;
	if (showflag) {
		do_showcluster(nodelist);
		exit(EXIT_SUCCESS);
	}
	do_command(argv, nodelist, allflag, username);
}

/*
 * This routine just rips open the various arrays and prints out information about
 * what the command would have done, and the topology of your cluster.  Invoked via
 * the -q switch.
 */

void do_showcluster(nodelist)
	char *nodelist[];
{
	int i, l;

	l = 0;

	if (rungroup != NULL)
		(void)printf("Rungroup: %s\n", rungroup);

	if (getenv("CLUSTER"))
		(void)printf("Cluster file: %s\n", getenv("CLUSTER"));
	for (i=0; nodelist[i] != NULL; i++) {
		if (rungroup != NULL) {
			if (test_node(i)) {
				l++;
				printf("Node: %3d Rungroup: %s Host: %s\n", l, grouplist[i], nodelist[i]);
			}
		} else {
			l++;
			printf("Node: %3d Rungroup: %s Host: %s\n", l, grouplist[i], nodelist[i]);
		}
	}
	printf("Command would run on node: %s\n", nodelist[check_rand(nodelist)]);
}

int check_rand(char *nodelist[])
{
	int i, g, n;

	srand48(getpid());
	if (rungroup == NULL) {
		for (n=0; nodelist[n] != NULL && test_node(n) == 0; n++)
			;
		for (i=n; nodelist[i] != NULL && test_node(i) == 1; i++)
			;
		g = (int)(drand48() * (float)(i-n));
		g += n;
	} else {
		for (i=0; nodelist[i] != NULL; i++)
			;
		g = (int)(drand48() * (float)i);
	}
	return(g);
}

/* 
 * Do the actual dirty work of the program, now that the arguments
 * have all been parsed out.
 */

void do_command(argv, nodelist, allrun, username)
	char **argv;
	char *nodelist[];
	char *username;
	int allrun;
{
	FILE *fd, *in;
	int out[2];
	int err[2];
	char buf[1024];
	int status, i, piping;
	char *p, *command, *rsh;

	extern int debug;

	i = 0;
	piping = 0;
	in = NULL;

	if (debug) {
		if (username != NULL)
			printf("As User: %s\n",username);
		printf("On node: %s", nodelist[check_rand(nodelist)]);
	} 

	buf[0] = '\0';
	command = strdup(buf);	/* construct the command from the remains of argv */
	for (p = *argv; p != NULL; p = *++argv ) {
		strcat(command, p);
		strcat(command, " ");
	}
	if (debug) {
		printf("\nDo Command: %s\n", command);
	}
	if (strcmp(command,"") == 0) {
		piping = 1;
		if (isatty(STDIN_FILENO) && piping)		/* are we a terminal?  then go interactive! */
			printf("seq>");
		in = fdopen(STDIN_FILENO, "r");
		command = fgets(buf, sizeof(buf), in);	/* start reading stuff from stdin and process */
		if (command != NULL)
			if (strcmp(command,"\n") == 0)
				command = NULL;
	}
	if (allrun)
		i = check_rand(nodelist);
	while (command != NULL) {
		if (!allrun)
			i = check_rand(nodelist);
#ifdef DEBUG
		printf("Working node: %d\n", i);
#endif
		pipe(out);	/* we set up pipes for each node, to prepare for the oncoming barrage of data */
		pipe(err);
		switch (fork()) {  /* its the ol fork and switch routine eh? */
			case -1:
				bailout(__LINE__);
				break;
			case 0: 
				if (dup2(out[1], STDOUT_FILENO) != STDOUT_FILENO) /* stupid unix tricks vol 1 */
					bailout(__LINE__);
				if (dup2(err[1], STDERR_FILENO) != STDERR_FILENO)
					bailout(__LINE__);
				if (close(out[0]) != 0)
					bailout(__LINE__);
				if (close(err[0]) != 0)
					bailout(__LINE__);
				rsh = getenv("RCMD_CMD");
				if (rsh == NULL)
					rsh = "rsh";
#ifdef DEBUG
				printf("%s %s %s\n", rsh, nodelist[i], command);
#endif
				if (username != NULL)  /* interestingly enough, this -l thing works great with ssh */
					execlp(rsh, rsh, "-l", username, nodelist[i], command, (char *)0);
				else
					execlp(rsh, rsh, nodelist[i], command, (char *)0);
				bailout(__LINE__);
		} /* end switch */
		if (close(out[1]) != 0)  /* now close off the useless stuff, and read the goodies */
			bailout(__LINE__);
		if (close(err[1]) != 0)
			bailout(__LINE__);
		fd = fdopen(out[0], "r"); /* stdout */
		while ((p = fgets(buf, sizeof(buf), fd)))
			printf("%s:\t%s", nodelist[i], p);
		fclose(fd);
		fd = fdopen(err[0], "r"); /* stderr */
		while ((p = fgets(buf, sizeof(buf), fd)))
			if (errorflag)
				printf("%s:\t%s", nodelist[i], p);
		fclose(fd);
		(void)wait(&status);
		if (piping) {
			if (isatty(STDIN_FILENO) && piping) /* yes, this is code repetition, no need to adjust your monitor */
				printf("seq>");
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

int test_node(int count)
{
	if (rungroup == NULL)
		return(1);
	else
		if (grouplist[count] != NULL)
			if (strcmp(rungroup,grouplist[count]) == 0)
				return(1);
	return(0);
}

/* Simple error handling routine, needs severe work.  Its almost totally useless. */

void bailout(line) 
	int line;
{
#ifdef DEBUG
	(void)fprintf(stderr, "Failed on line %d\n", line);
#else
	(void)fprintf(stderr, "Internal error, aborting\n");
#endif
	exit(EXIT_FAILURE);
}
