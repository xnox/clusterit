/* $Id: dsh.c,v 1.5 1998/10/20 07:28:44 garbled Exp $ */
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

#if !defined(lint) && defined(__NetBSD__)
__COPYRIGHT(
"@(#) Copyright (c) 1998\n\
        Tim Rightnour.  All rights reserved\n");
#endif /* not lint */

#if !defined(lint) && defined(__NetBSD__)
__RCSID("$Id: dsh.c,v 1.5 1998/10/20 07:28:44 garbled Exp $");
#endif

#define MAX_CLUSTER 512
#define DEFAULT_FANOUT 64
#define MAX_GROUPS 32

extern int errno;
#ifdef __NetBSD__
void bailout __P((int));
void do_command __P((char **argv, char *nodelist[], int fanout, char *username));
void do_showcluster __P((char *nodelist[], int fanout));
int test_node __P((int count));
#else
void bailout(int);
void do_command(char **argv, char *nodelist[], int fanout, char *username);
char * strsep(char **stringp, const char *delim);
void do_showcluster(char *nodelist[], int fanout);
int test_node(int count);
#endif

int debug;
int errorflag;
char *grouplist[MAX_CLUSTER];
char *rungroup[MAX_GROUPS];

/* 
 *  dsh is a cluster management tool derrived from the IBM tool of the
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
	int someflag, ch, i, fanout, showflag, exclusion, j, fail, fanflag;
	char *p, *group, *nodelist[MAX_CLUSTER], *nodename, *clusterfile, *username;
	char *exclude[MAX_CLUSTER];
	char buf[256];

	extern int debug;
	extern int errorflag;

	someflag = 0;
	showflag = 0;
	fanflag = 0;
	exclusion = 0;
	debug = 0;
	errorflag = 0;
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
			    "usage: dsh [-eiq] [-f fanout] [-g rungroup1,...,rungroupN] [-l username] [-x node1,...,nodeN] [-w node1,..,nodeN] [command ...]\n");
			exit(EXIT_FAILURE);
			break;
		default:
			break;
	}
	if (!fanflag)	/* check for a fanout var, and use it if the fanout isn't on the commandline. */
		if (getenv("FANOUT"))
			fanout = atoi(getenv("FANOUT"));
	if (!someflag) { /* if -w wasn't specified, we need to parse the cluster file */
		clusterfile = getenv("CLUSTER");
		if (clusterfile == NULL) {
			(void)fprintf(stderr, "must use -w flag without CLUSTER environment setting.\n");
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
				}
		}
		nodelist[i] = '\0';
		grouplist[i] = '\0';
		fclose(fd);
	}
	argc -= optind;
	argv += optind;
	if (showflag) {
		do_showcluster(nodelist, fanout);
		exit(EXIT_SUCCESS);
	}
	do_command(argv, nodelist, fanout, username);
	exit(EXIT_SUCCESS);
}

/*
 * This routine just rips open the various arrays and prints out information about
 * what the command would have done, and the topology of your cluster.  Invoked via
 * the -q switch.
 */

void do_showcluster(nodelist, fanout)
	char *nodelist[];
	int fanout;
{
	int i, j, l, n;

	i = l = 0;
	if (rungroup[0] == NULL)
		for (i=0; nodelist[i] != NULL; i++)		/* just count the nodes */
			;
	else
		for (j=0; nodelist[j] != NULL; j++)
			if (test_node(j))
				i++;

	j = i / fanout;		/* how many times do I have to run in order to reach them all */
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
		for (i=n * fanout; ((nodelist[i] != NULL) && (i < (n + 1) * fanout)); i++) {
			if (rungroup[0] != NULL) {
				if (test_node(i)) {
					l++;
					if (grouplist[i] == NULL)
						(void)printf("Node: %3d\tFangroup: %3d\tRungroup: None\tHost: %s\n", l, n + 1, nodelist[i]);
					else
						(void)printf("Node: %3d\tFangroup: %3d\tRungroup: %s\tHost: %s\n", l, n + 1, grouplist[i], nodelist[i]);
				}
			} else {
				l++;
				if (grouplist[i] == NULL)
					(void)printf("Node: %3d\tFangroup: %3d\tRungroup: None\tHost: %s\n", l, n + 1, nodelist[i]);
				else
					(void)printf("Node: %3d\tFangroup: %3d\tRungroup: %s\tHost: %s\n", l, n + 1, grouplist[i], nodelist[i]);
			}
		}
	}
}

/* 
 * Do the actual dirty work of the program, now that the arguments
 * have all been parsed out.
 */

void do_command(argv, nodelist, fanout, username)
	char **argv;
	char *nodelist[];
	char *username;
	int fanout;
{
	FILE *fd, *in;
	int out[fanout][2];
	int err[fanout][2];
	char buf[1024];
	int status, i, j, n, g, piping;
	char *p, *command, *rsh, *cd;

	extern int debug;

	j = i = 0;
	piping = 0;
	in = NULL;
	cd = NULL;

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
	} else {
		if (rungroup[0] == NULL)
			for (i=0; nodelist[i] != NULL; i++)		/* just count the nodes */
				;
		else
			for (j=0; nodelist[j] != NULL; j++)
				if (test_node(j))
						i++;
	}
	j = i / fanout;
	if (i % fanout)
		j++;

	buf[0] = '\0';
	command = strdup(buf);	/* construct the command from the remains of argv */
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
		if (isatty(STDIN_FILENO) && piping)		/* are we a terminal?  then go interactive! */
			(void)printf("dsh>");
		in = fdopen(STDIN_FILENO, "r");
		command = fgets(buf, sizeof(buf), in);	/* start reading stuff from stdin and process */
		if (command != NULL)
			if (strcmp(command,"\n") == 0)
				command = NULL;
	}

	while (command != NULL) {
		for (n=0; n <= j; n++) {
			for (i=n * fanout; ((nodelist[i] != NULL) && (i < (n + 1) * fanout)); i++) {
				if (test_node(i)) {
					g = i - n * fanout;
#ifdef DEBUG
					(void)printf("Working node: %d, group %d, fanout part: %d\n", i, n, g);
#endif
					pipe(out[i]);	/* we set up pipes for each node, to prepare for the oncoming barrage of data */
					pipe(err[i]);
					switch (fork()) {  /* its the ol fork and switch routine eh? */
						case -1:
							bailout(__LINE__);
							break;
						case 0: 
							if (dup2(out[g][1], STDOUT_FILENO) != STDOUT_FILENO) /* stupid unix tricks vol 1 */
								bailout(__LINE__);
							if (dup2(err[g][1], STDERR_FILENO) != STDERR_FILENO)
								bailout(__LINE__);
							if (close(out[g][0]) != 0)
								bailout(__LINE__);
							if (close(err[g][0]) != 0)
								bailout(__LINE__);
							rsh = getenv("RCMD_CMD");
							if (rsh == NULL)
								rsh = "rsh";
#ifdef DEBUG
							(void)printf("%s %s %s\n", rsh, nodelist[i], command);
#endif
							if (username != NULL)  /* interestingly enough, this -l thing works great with ssh */
								execlp(rsh, rsh, "-l", username, nodelist[i], command, (char *)0);
							else
								execlp(rsh, rsh, nodelist[i], command, (char *)0);
							bailout(__LINE__);
						}
				} /* test_node */
			} /* for i */
			for (i=n * fanout; ((nodelist[i] != NULL) && (i < (n + 1) * fanout)); i++) {
				if (test_node(i)) {
					g = i - n * fanout;
					if (close(out[g][1]) != 0)  /* now close off the useless stuff, and read the goodies */
						bailout(__LINE__);
					if (close(err[g][1]) != 0)
						bailout(__LINE__);
					fd = fdopen(out[g][0], "r"); /* stdout */
					while ((p = fgets(buf, sizeof(buf), fd)))
						(void)printf("%s:\t%s", nodelist[i], p);
					fclose(fd);
					fd = fdopen(err[g][0], "r"); /* stderr */
					while ((p = fgets(buf, sizeof(buf), fd)))
						if (errorflag)
							(void)printf("%s:\t%s", nodelist[i], p);
					fclose(fd);
					(void)wait(&status);
				} /* test_node */
			} /* for pipe read */
		} /* for n */
		if (piping) {
			if (isatty(STDIN_FILENO) && piping) /* yes, this is code repetition, no need to adjust your monitor */
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

int test_node(int count)
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
