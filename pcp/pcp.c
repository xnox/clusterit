/* $Id: pcp.c,v 1.11 2003/11/02 15:33:17 garbled Exp $ */
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

#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include "../common/common.h"

#if !defined(lint) && defined(__NetBSD__)
__COPYRIGHT(
"@(#) Copyright (c) 1998, 1999, 2000\n\
        Tim Rightnour.  All rights reserved.\n");
__RCSID("$Id: pcp.c,v 1.11 2003/11/02 15:33:17 garbled Exp $");
#endif

extern int errno;

void do_copy __P((char **argv, int recurse, int preserve, char *username));
void paralell_copy __P((char *rcp, char *args, char *source_file,
	char *destination_file));
void serial_copy __P((char *rcp, char *args, char *source_file,
	char *destination_file));

char **lumplist;
char **rungroup;
char *progname;
int fanout, concurrent, quiet, debug, grouping, exclusion;
node_t *nodelink;
group_t *grouplist;

/* 
 *  pcp is a cluster management tool based on the IBM tool of the
 *  same name.  It allows a user, or system administrator to copy files
 *  to a cluster of machines with a single command.
 */

int
main(int argc, char *argv[])
{
	extern char *optarg;
	extern int optind;

	int someflag, ch, i, preserve, recurse;
	char *p, *q, *nodename, *username, *group;
	char **exclude, **grouptemp;
	node_t *nodeptr;

	fanout = 0;
	quiet = 1;
	concurrent = 0;
	someflag = 0;
	preserve = 0;
	recurse = 0;
	exclusion = 0;
	grouping = 0;
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

#if defined(__linux__)
	while ((ch = getopt(argc, argv, "+?cdeprf:g:l:w:x:")) != -1)
#else
	while ((ch = getopt(argc, argv, "?cdeprf:g:l:w:x:")) != -1)
#endif
		switch (ch) {
		case 'c':		/* set concurrent mode */
			concurrent = 1;
			break;
		case 'd':		/* hidden debug mode */
			debug = 1;
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
			i = 0;
			for (p = optarg; p != NULL; ) {
				nodename = (char *)strsep(&p, ",");
				if (nodename != NULL)
					(void)nodealloc(nodename);
			}
			break;
		case '?':
			(void)fprintf(stderr,
				"usage: %s [-cepr] [-f fanout] [-g rungroup1,...,rungroupN] "
				"[-l username] [-x node1,...,nodeN] [-w node1,..,nodeN] "
				"source_file1 [source_file2 ... source_fileN] "
				"[desitination_file]\n", progname);
			return(EXIT_FAILURE);
			/* NOTREACHED */
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
	if (!someflag)
		parse_cluster(exclude);

	argc -= optind;
	argv += optind;
	do_copy(argv, recurse, preserve, username);
	return(EXIT_SUCCESS);
}

/* 
 * Do the actual dirty work of the program, now that the arguments
 * have all been parsed out.
 */

void do_copy(argv, recurse, preserve, username)
	char **argv;
	int recurse, preserve;
	char *username;
{
	int numsource, j;
	char args[64];
	char *cargs, *source_file, *destination_file, *tempstore, *rcp;
	node_t *nodeptr;

	if (debug) {
		j = 0;
		if (username != NULL)
			(void)printf("As User: %s\n", username);
		(void)printf("On nodes:\n");
		for (nodeptr = nodelink; nodeptr; nodeptr = nodeptr->next) {
			if (!(j % 4) && j > 0)
				(void)printf("\n");
			(void)printf("%s\t", nodeptr->name);
			j++;
		}
	}
	if (*argv == (char *) NULL) {
		(void)fprintf(stderr, "Must specify at least one file to copy\n");
		exit(EXIT_FAILURE);
	}
	tempstore = NULL;
	source_file = strdup(*argv);
	numsource = 1;
	while (*++argv != NULL) {
		numsource++;
		if (tempstore != NULL) {
			source_file = realloc(source_file,
			    ((strlen(source_file)+2+strlen(tempstore)) *
			    sizeof(char)));
			if (source_file == NULL)
				bailout(__LINE__);
			source_file = strcat(source_file, " ");
			source_file = strdup(strcat(source_file, tempstore));
		}
		tempstore = strdup(*argv);
	}
	if (numsource == 1)
		destination_file = strdup(source_file);
	else
		destination_file = strdup(tempstore);

	if (debug)
		printf("\nDo Copy: %s %s\n", source_file, destination_file);

	rcp = getenv("RCP_CMD");
	if (rcp == NULL)
		rcp = strdup("rcp");
	if (rcp == NULL)
		bailout(__LINE__);
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
		paralell_copy(rcp, cargs, source_file, destination_file);
	else
		serial_copy(rcp, cargs, source_file, destination_file);
}

/* Copy files in paralell.  This is preferred with smaller files, because
   the initial connection and authentication latency is longer than the
   file transfer.  With large files, you generate more collisions than
   good packets, and actually slow it down, thus serial is faster. */

void
paralell_copy(rcp, args, source_file, destination_file)
	char *rcp, *args, *source_file, *destination_file;
{
	int i, j, n, g, status;
	char buf[MAXBUF], pipebuf[2048], *cd, *p;
	FILE *fd, *in;
	char *argz[51], **aps;
	node_t *nodeptr, *nodehold;
	size_t maxnodelen;
	pid_t currentchild;

	j = i = maxnodelen = 0;
	in = NULL;
	cd = pipebuf;

	for (nodeptr = nodelink; nodeptr; nodeptr = nodeptr->next) {
		if (strlen(nodeptr->name) > maxnodelen)
			maxnodelen = strlen(nodeptr->name);
		j++;
	}

	i = j;
	j = i / fanout;
	if (i % fanout)
		j++;

	g = 0;
	nodeptr = nodelink;
	for (n=0; n <= j; n++) {
		nodehold = nodeptr;
		for (i=0; (i < fanout && nodeptr != NULL); i++) {
			g++;
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
			if (debug)
				printf("%s %s %s %s:%s\n", rcp, args, source_file,
					nodeptr->name, destination_file);
			nodeptr->childpid = fork();
			switch (nodeptr->childpid) {
			case -1:
				bailout(__LINE__);
				break;
			case 0:
				if (dup2(nodeptr->out.fds[1], STDOUT_FILENO) != STDOUT_FILENO)
					bailout(__LINE__);
				if (dup2(nodeptr->err.fds[1], STDERR_FILENO) != STDERR_FILENO)
					bailout(__LINE__);
				if (close(nodeptr->out.fds[0]) != 0)
					bailout(__LINE__);
				if (close(nodeptr->err.fds[0]) != 0)
					bailout(__LINE__);
				(void)sprintf(buf, "%s %s %s %s:%s", rcp, args,
					source_file, nodeptr->name, destination_file);
				p = strdup(buf);
				for (aps = argz; (*aps = strsep(&p, " ")) != NULL;)
					if (**aps != '\0')
						++aps;
				execvp(rcp, argz);
				bailout(__LINE__);
			default:
				break;
			} /* switch */
			nodeptr = nodeptr->next;
		} /* for i */
		nodeptr = nodehold;
		for (i=0; (i < fanout && nodeptr != NULL); i++) {
			currentchild = nodeptr->childpid;
		/* now close off the useless stuff, and read the goodies */
			if (close(nodeptr->out.fds[1]) != 0)
				bailout(__LINE__);
			if (close(nodeptr->err.fds[1]) != 0)
				bailout(__LINE__);
			fd = fdopen(nodeptr->out.fds[0], "r"); /* stdout */
			if (fd == NULL)
				bailout(__LINE__);
			while ((cd = fgets(pipebuf, sizeof(pipebuf), fd)))
				if (cd != NULL && !quiet)
					(void)printf("%s: %s",
						alignstring(nodeptr->name, maxnodelen), cd);
			fclose(fd);
			fd = fdopen(nodeptr->err.fds[0], "r"); /* stderr */
			if (fd == NULL)
				bailout(__LINE__);
			while ((cd = fgets(pipebuf, sizeof(pipebuf), fd)))
				if (cd != NULL && !quiet)
					(void)printf("%s: %s",
						alignstring(nodeptr->name, maxnodelen), cd);
			fclose(fd);
			(void)wait(&status);
			nodeptr = nodeptr->next;
		} /* pipe read */
	} /* for n */	
}

/* serial copy */

void
serial_copy(rcp, args, source_file, destination_file)
	char *rcp, *args, *source_file, *destination_file;
{
	node_t *nodeptr;
	char buf[MAXBUF], *command;

	for (nodeptr=nodelink; nodeptr != NULL; nodeptr = nodeptr->next) {
		(void)sprintf(buf, "%s %s %s %s:%s", rcp, args, source_file,
			nodeptr->name, destination_file);
		command = strdup(buf);
		system(command);
	}
}
