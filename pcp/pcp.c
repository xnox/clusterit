/* $Id: pcp.c,v 1.5 1998/12/14 17:59:57 garbled Exp $ */
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

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#if !defined(lint) && defined(__NetBSD__)
__COPYRIGHT(
"@(#) Copyright (c) 1998\n\
        Tim Rightnour.  All rights reserved.\n");
#endif /* not lint */

#if !defined(lint) && defined(__NetBSD__)
__RCSID("$Id: pcp.c,v 1.5 1998/12/14 17:59:57 garbled Exp $");
#endif

#define MAX_CLUSTER 512
#define DEFAULT_FANOUT 64
#define MAX_GROUPS 32

extern int errno;
#ifdef __NetBSD__
void do_copy __P((char **argv, char *nodelist[], int recurse, int preserve, char *username));
void paralell_copy __P((char *rcp, char *args, char *nodelist[], char *source_file, char *destination_file));
void serial_copy __P((char *rcp, char *args, char *nodelist[], char *source_file, char *destination_file));
int test_node __P((int count));
void bailout __P((int));
#else
void do_copy(char **argv, char *nodelist[], int recurse, int preserve, char *username);
void paralell_copy(char *rcp, char *args, char *nodelist[], char *source_file, char *destination_file);
void serial_copy(char *rcp, char *args, char *nodelist[], char *source_file, char *destination_file);
char * strsep(char **stringp, const char *delim);
int test_node(int count);
void bailout(int);
#endif

char *grouplist[MAX_CLUSTER];
char *rungroup[MAX_GROUPS];
int fanout, concurrent, quiet;

/* 
 *  pcp is a cluster management tool based on the IBM tool of the
 *  same name.  It allows a user, or system administrator to copy files
 *  to a cluster of machines with a single command.
 */

int
main(argc, argv) 
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;

	FILE *fd;
	int someflag, ch, i, preserve, recurse, exclusion, j, fail;
	char *p, *nodelist[MAX_CLUSTER], *nodename, *clusterfile, *username, *group;
	char buf[256];
	char *exclude[MAX_CLUSTER];

	fanout = 0;
	quiet = 1;
	concurrent = 0;
	someflag = 0;
	preserve = 0;
	recurse = 0;
	exclusion = 0;
	username = NULL;
	group = NULL;
	for (i=0; i < MAX_GROUPS; i++)
		rungroup[i] = NULL;

	while ((ch = getopt(argc, argv, "?ceprf:g:l:w:x:")) != -1)
		switch (ch) {
		case 'c':		/* set concurrent mode */
			concurrent = 1;
			break;
		case 'e':		/* display error messages */
			quiet = 0;
			break;
		case 'p':		/* preserve file modes */
			preserve = 1;
			break;
		case 'r':		/* recursive directory operations */
			recurse = 1;
			break;
		case 'l':               /* invoke me as some other user */
			username = strdup(optarg);
			break;
		case 'f':		/* set the fanout size */
			fanout = atoi(optarg);
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
		case '?':
			(void)fprintf(stderr, "usage: pcp [-cepr] [-f fanout] [-g rungroup1,...,rungroupN] [-l username] [-x node1,...,nodeN] [-w node1,..,nodeN] source_file [desitination_file]\n");
			exit(EXIT_FAILURE);
			break;
		default:
			break;
	}
	if (fanout == 0) {
		if (getenv("FANOUT"))
			fanout = atoi(getenv("FANOUT"));
		else
			fanout = DEFAULT_FANOUT;
	}
	if (!someflag) {
		clusterfile = getenv("CLUSTER");
		if (clusterfile == NULL) {
			(void)fprintf(stderr, "must use -w flag without CLUSTER environment setting.\n");
			exit(EXIT_FAILURE);
		}
		fd = fopen(clusterfile, "r");
		i = 0;
		while ((nodename = fgets(buf, sizeof(buf), fd)) && i < MAX_CLUSTER - 1) {
			p = (char *)strsep(&nodename, "\n");
			if (strcmp(p, "") != 0) {
				if (exclusion) {		/* this handles the -x args */
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
		}
		nodelist[i] = '\0';
		grouplist[i] = '\0';
		fclose(fd);
	}
	argc -= optind;
	argv += optind;
	do_copy(argv, nodelist, recurse, preserve, username);
	return(EXIT_SUCCESS);
}

/* 
 * Do the actual dirty work of the program, now that the arguments
 * have all been parsed out.
 */

void do_copy(argv, nodelist, recurse, preserve, username)
	char **argv;
	char *nodelist[];
	int recurse, preserve;
	char *username;
{
	char args[64];
	char *cargs, *source_file, *destination_file, *rcp;

#ifdef DEBUG
	int i;

	printf ("On nodes: ");
	for (i=0; nodelist[i] != NULL; i++)
		printf("%s ", nodelist[i]);
#endif
	if (*argv == (char *) NULL) {
		(void)fprintf(stderr, "Must specify at least one file to copy\n");
		exit(EXIT_FAILURE);
	}
	source_file = strdup(*argv);
	if (*++argv != NULL) {
		destination_file = strdup(*argv);
	} else 
		destination_file = strdup(source_file);

#ifdef DEBUG
	printf("\nDo Copy: %s %s\n", source_file, destination_file);
#endif
	rcp = getenv("RCP_CMD");
	if (rcp == NULL)
		rcp = "rcp";
	(void)sprintf(args, " ");
	if (recurse)
		strcat(args, "-r ");
	if (preserve)
		strcat(args, "-p ");
	if (username != NULL) {
		strcat(args, "-l ");
		strcat(args, username);
		strcat(args, " ");
	}
	cargs = strdup(args);

	if (concurrent)
		paralell_copy(rcp, cargs, nodelist, source_file, destination_file);
	else
		serial_copy(rcp, cargs, nodelist, source_file, destination_file);
}

/* Copy files in paralell.  This is preferred with smaller files, because
   the initial connection and authentication latency is longer than the
   file transfer.  With large files, you generate more collisions than
   good packets, and actually slow it down, thus serial is faster. */

void
paralell_copy(rcp, args, nodelist, source_file, destination_file)
	char *rcp, *args, *nodelist[], *source_file, *destination_file;
{
	int i, j, n, g, status;
	char buf[512], pipebuf[2048], *cd, *p;
	FILE *fd, *in;
	int out[fanout+1][2];
	int err[fanout+1][2];
	char *argz[51], **aps;

	j = i = 0;
	in = NULL;
	cd = pipebuf;

	for (i=0; nodelist[i] != NULL; i++)             /* just count the nodes */
		;
	j = i / fanout;
	if (i % fanout)
		j++;

	for (n=0; n <= j; n++) {
		for (i=n * fanout; ((nodelist[i] != NULL) && (i < (n + 1) * fanout)); i++) {
			if (test_node(i)) {
				g = i - n * fanout;
/*
 * we set up pipes for each node, to prepare for the oncoming barrage of data.
 * Close on exec must be set here, otherwise children spawned after other
 * children, inherit the open file descriptors, and cause the pipes to remain open
 * forever.
 */
				if (pipe(out[g]) != 0)
					bailout(__LINE__);
				if (pipe(err[g]) != 0)
					bailout(__LINE__);
				if (fcntl(out[g][0], F_SETFD, 1) == -1)
					bailout(__LINE__);
				if (fcntl(out[g][1], F_SETFD, 1) == -1)
					bailout(__LINE__);
				if (fcntl(err[g][0], F_SETFD, 1) == -1)
					bailout(__LINE__);
				if (fcntl(err[g][1], F_SETFD, 1) == -1)
					bailout(__LINE__);
#ifdef DEBUG
				printf("%s %s %s %s %s:%s\n", rcp, args, source_file, nodelist[i], destination_file);
#endif
				switch (fork()) {
					case -1:
						bailout(__LINE__);
						break;
					case 0:
						if (dup2(out[g][1], STDOUT_FILENO) != STDOUT_FILENO)
							bailout(__LINE__);
						if (dup2(err[g][1], STDERR_FILENO) != STDERR_FILENO)
							bailout(__LINE__);
						if (close(out[g][0]) != 0)
							bailout(__LINE__);
						if (close(err[g][0]) != 0)
							bailout(__LINE__);
						(void)sprintf(buf, "%s %s %s %s:%s", rcp, args, source_file, nodelist[i], destination_file);
						p = strdup(buf);
						for (aps = argz; (*aps = strsep(&p, " ")) != NULL;)
							if (**aps != '\0')
								++aps;
						execvp(rcp, argz);
						bailout(__LINE__);
					default:
						break;
				} /* switch */
			} /* test */
		} /* for i */
		for (i=n * fanout; ((nodelist[i] != NULL) && (i < (n + 1) * fanout)); i++) {
			if (test_node(i)) {
				g = i - n * fanout;
				if (close(out[g][1]) != 0)  /* now close off the useless stuff, and read the goodies */
					bailout(__LINE__);
				if (close(err[g][1]) != 0)
					bailout(__LINE__);
				fd = fdopen(out[g][0], "r"); /* stdout */
					if (fd == NULL)
				while ((cd = fgets(pipebuf, sizeof(pipebuf), fd)))
					if (cd != NULL && !quiet)
						(void)printf("%-12s: %s", nodelist[i], cd);
				fclose(fd);
				fd = fdopen(err[g][0], "r"); /* stderr */
				if (fd == NULL)
					bailout(__LINE__);
				while ((cd = fgets(pipebuf, sizeof(pipebuf), fd)))
					if (cd != NULL && !quiet)
						(void)printf("%-12s: %s", nodelist[i], cd);
				fclose(fd);
				(void)wait(&status);
			} /* test */
		} /* pipe read */
	} /* for n */	
}

/* serial copy */

void
serial_copy(rcp, args, nodelist, source_file, destination_file)
	char *rcp, *args, *nodelist[], *source_file, *destination_file;
{
	int i;
	char buf[512], *command;

	for (i=0; nodelist[i] != NULL; i++)
		if (test_node(i)) {
			(void)sprintf(buf, "%s %s %s %s:%s", rcp, args, source_file, nodelist[i], destination_file);
			command = strdup(buf);
			system(command);
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
				if (strcmp(rungroup[i],grouplist[count]) == 0)
					return(1);
	return(0);
}

/* Simple error handling routine, needs severe work.  Its almost totally useless. */

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

