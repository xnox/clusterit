/* $Id: run.c,v 1.11 2004/10/04 18:22:25 garbled Exp $ */
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

#include <sys/types.h>
#include <sys/wait.h>

#include "../common/common.h"

#if !defined(lint) && defined(__NetBSD__)
__COPYRIGHT(
"@(#) Copyright (c) 1998, 1999, 2000\n\
        Tim Rightnour.  All rights reserved\n");
__RCSID("$Id: run.c,v 1.11 2004/10/04 18:22:25 garbled Exp $");
#endif

extern int errno;

void do_command(char **argv, int allrun, char *username);
node_t *check_rand(void);

/* globals */

int debug, exclusion, grouping;
int errorflag;
char **rungroup;
char **lumplist;
char *progname;
node_t *nodelink;
group_t *grouplist;

/* 
 *  run is a cluster management tool derrived from the IBM tool of the
 *  same name.  It allows a user, or system administrator to issue
 *  commands in paralell on a group of machines.
 */

int
main(int argc, char **argv)
{
    extern char *optarg;
    extern int optind;

    int someflag, ch, i, allflag, showflag;
    char *p, *q, *group, *nodename, *username;
    char **exclude, **grouptemp;
    node_t *nodeptr;

    someflag = 0;
    showflag = 0;
    exclusion = 0;
    debug = 0;
    errorflag = 0;
    allflag = 0;
    grouping = 0;
    username = NULL;
    nodename = NULL;
    group = NULL;
    nodeptr = NULL;
    nodelink = NULL;

    rungroup = malloc(sizeof(char **) * GROUP_MALLOC);
    if (rungroup == NULL)
	bailout();
    exclude = malloc(sizeof(char **) * GROUP_MALLOC);
    if (exclude == NULL)
	bailout();

    progname = p = q = argv[0];
    while (progname != NULL) {
	q = progname;
	progname = (char *)strsep(&p, "/");
    }
    progname = strdup(q);

    srand48(getpid()); /* seed the random number generator */

#if defined(__linux__)
    while ((ch = getopt(argc, argv, "+?adeiqg:l:w:x:")) != -1)
#else
	while ((ch = getopt(argc, argv, "?adeiqg:l:w:x:")) != -1)
#endif
	    switch (ch) {
	    case 'a':		/* set the allrun flag */
		allflag = 1;
		break;
	    case 'd':       /* set the debug flag */
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
				bailout();
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
				bailout();
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
	    case '?':		/* you blew it */
		(void)fprintf(stderr,
		    "usage: %s [-aeiq] [-g rungroup1,...,rungroupN] "
		    "[-l username] [-x node1,...,nodeN] [-w node1,..,nodeN] "
		    "[command ...]\n", progname);
		exit(EXIT_FAILURE);
		break;
	    default:
		break;
	    }
    if (!someflag)
	parse_cluster(exclude);

    argc -= optind;
    argv += optind;
    if (showflag) {
	do_showcluster(DEFAULT_FANOUT);
	exit(EXIT_SUCCESS);
    }
    do_command(argv, allflag, username);
    exit(EXIT_SUCCESS);
}

node_t *
check_rand(void)
{
    int i, g;
    node_t *nodeptr;

    for (i = 0, nodeptr = nodelink; nodeptr != NULL; nodeptr = nodeptr->next)
	i++;

    g = (int)(lrand48() % (long)i);

    for (i = 0, nodeptr = nodelink; ((nodeptr != NULL) && (i != g));
	 nodeptr = nodeptr->next)
	i++;

    return(nodeptr);
}

/* 
 * Do the actual dirty work of the program, now that the arguments
 * have all been parsed out.
 */

void
do_command(char **argv, int allrun, char *username)
{
    FILE *fd, *in;
    char buf[MAXBUF];
    int status, i, piping;
    char *p, *command, *rsh;
    node_t *nodeptr;
    size_t maxnodelen;

    i = 0;
    piping = 0;
    in = NULL;
    maxnodelen = 0;

    for (nodeptr = nodelink; nodeptr != NULL; nodeptr = nodeptr->next)
	if (strlen(nodeptr->name) > maxnodelen)
	    maxnodelen = strlen(nodeptr->name);

    if (debug)
	if (username != NULL)
	    (void)printf("As User: %s\n", username);

    /* construct the command from the remains of argv */
    command = (char *)malloc(MAXBUF * sizeof(char));
    memset(command, 0, MAXBUF * sizeof(char));
    for (p = *argv; p != NULL; p = *++argv ) {
	strcat(command, p);
	strcat(command, " ");
    }
    if (debug) {
	(void)printf("\nDo Command: %s\n", command);
    }
    if (strcmp(command,"") == 0) {
	piping = 1;
	if (isatty(STDIN_FILENO) && piping)
/* are we a terminal?  then go interactive! */
	    (void)printf("%s>", progname);
	in = fdopen(STDIN_FILENO, "r");
	command = fgets(buf, sizeof(buf), in);
/* start reading stuff from stdin and process */
	if (command != NULL)
	    if (strcmp(command,"\n") == 0)
		command = NULL;
    }
    if (allrun)
	nodeptr = check_rand();
    while (command != NULL) {
	if (!allrun)
	    nodeptr = check_rand();
	if (debug)
	    printf("Working node: %s\n", nodeptr->name);
	if (pipe(nodeptr->out.fds) != 0)
	    bailout();
	if (pipe(nodeptr->err.fds) != 0)
	    bailout();
/* we set up pipes for each node, to prepare
 * for the oncoming barrage of data.
 */
	nodeptr->childpid = fork();
	switch (nodeptr->childpid) {
/* its the ol fork and switch routine eh? */
	case -1:
	    bailout();
	    break;
	case 0: 
	    if (dup2(nodeptr->out.fds[1], STDOUT_FILENO) != STDOUT_FILENO)
/* stupid unix tricks vol 1 */
		bailout();
	    if (dup2(nodeptr->err.fds[1], STDERR_FILENO) != STDERR_FILENO)
		bailout();
	    if (close(nodeptr->out.fds[0]) != 0)
		bailout();
	    if (close(nodeptr->err.fds[0]) != 0)
		bailout();
	    rsh = getenv("RCMD_CMD");
	    if (rsh == NULL)
		rsh = strdup("rsh");
	    if (rsh == NULL)
		bailout();
	    if (debug)
		printf("%s %s %s\n", rsh, nodeptr->name, command);
	    if (username != NULL)
/* interestingly enough, this -l thing works great with ssh */
		execlp(rsh, rsh, "-l", username, nodeptr->name,
		    command, (char *)0);
	    else
		execlp(rsh, rsh, nodeptr->name, command, (char *)0);
	    bailout();
	} /* end switch */
	if (close(nodeptr->out.fds[1]) != 0)
/* now close off the useless stuff, and read the goodies */
	    bailout();
	if (close(nodeptr->err.fds[1]) != 0)
	    bailout();
	fd = fdopen(nodeptr->out.fds[0], "r"); /* stdout */
	while ((p = fgets(buf, sizeof(buf), fd)))
	    (void)printf("%*s: %s", -maxnodelen, nodeptr->name, p);
	fclose(fd);
	fd = fdopen(nodeptr->err.fds[0], "r"); /* stderr */
	while ((p = fgets(buf, sizeof(buf), fd)))
	    if (errorflag)
		(void)printf("%s: %s", -maxnodelen, nodeptr->name, p);
	fclose(fd);
	(void)wait(&status);
	if (piping) {
	    if (isatty(STDIN_FILENO) && piping)
/* yes, this is code repetition, no need to adjust your monitor */
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
