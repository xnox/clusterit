/* $Id: run.c,v 1.22 2007/04/02 18:49:08 garbled Exp $ */
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
#include <poll.h>
#include <signal.h>
#include <libgen.h>
#include <fcntl.h>

#include "../common/common.h"

#if !defined(lint) && defined(__NetBSD__)
__COPYRIGHT(
"@(#) Copyright (c) 1998, 1999, 2000\n\
        Tim Rightnour.  All rights reserved\n");
__RCSID("$Id: run.c,v 1.22 2007/04/02 18:49:08 garbled Exp $");
#endif

extern int errno;

void do_command(char **argv, int allrun, char *username);
node_t *check_rand(void);

/* globals */

int debug, exclusion, grouping;
int errorflag, nrofrungroups;
int testflag, rshport, porttimeout;
char **rungroup;
char **lumplist;
char *progname;
node_t *nodelink;
group_t *grouplist;
volatile sig_atomic_t alarmtime;

/* 
 *  run is a cluster management tool. The tool will run a command remotely
 *  on a single remote note, chosen at random.
 */

int
main(int argc, char **argv)
{
    extern char *optarg;
    extern int optind;
    extern char *version;

    int someflag, ch, i, allflag, showflag;
    char *p, *group, *nodename, *username;
    char **exclude;
    node_t *nodeptr;

    someflag = 0;
    showflag = 0;
    exclusion = 0;
    debug = 0;
    errorflag = 0;
    allflag = 0;
    grouping = 0;
    nrofrungroups = 0;
    testflag = 0;
    rshport = 0;
    porttimeout = 5; /* 5 seconds to port timeout */
    username = NULL;
    nodename = NULL;
    group = NULL;
    nodeptr = NULL;
    nodelink = NULL;
    exclude = NULL;
    
    rungroup = calloc(GROUP_MALLOC, sizeof(char **));
    if (rungroup == NULL)
	bailout();

    progname = strdup(basename(argv[0]));

    srand48(getpid()); /* seed the random number generator */

#if defined(__linux__)
    while ((ch = getopt(argc, argv, "+?adeiqtvg:l:o:p:w:x:")) != -1)
#else
    while ((ch = getopt(argc, argv, "?adeiqtvg:l:o:p:w:x:")) != -1)
#endif
	switch (ch) {
	case 'a':		/* set the allrun flag */
	    allflag = 1;
	    break;
	case 'd':               /* set the debug flag */
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
	case 't':           /* test the nodes before connecting */
	    testflag = 1;
	    break;
	case 'p':           /* what is the rsh port number? */
	    rshport = atoi(optarg);
	    break;
	case 'o':               /* set the test timeout in seconds */
	    porttimeout = atoi(optarg);
	    break;
	case 'g':		/* pick a group to run on */
	    grouping = 1;
	    nrofrungroups = parse_gopt(optarg);
	    break;			
	case 'x':		/* exclude nodes, w overrides this */
	    exclusion = 1;
	    exclude = parse_xopt(optarg);
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
	case 'v':
	    (void)printf("%s: %s\n", progname, version);
	    exit(EXIT_SUCCESS);
	    break;
	case '?':		/* you blew it */
	    (void)fprintf(stderr,
	        "usage: %s [-aeiqtv] [-p portnum] [-o timeout] "
		"[-g rungroup1,...,rungroupN]\n"
		"\t[-l username] [-x node1,...,nodeN] [-w node1,..,nodeN] "
		"[command ...]\n", progname);
	    exit(EXIT_FAILURE);
	    break;
	default:
	    break;
	}

    if (username == NULL && getenv("RCMD_USER"))
	username = strdup(getenv("RCMD_USER"));
    if (!rshport && getenv("RCMD_PORT"))
	rshport = atoi(getenv("RCMD_PORT"));
    if (!testflag && getenv("RCMD_TEST"))
	testflag = 1;
    if (porttimeout == 5 && getenv("RCMD_TEST_TIMEOUT"))
	porttimeout = atoi(getenv("RCMD_TEST_TIMEOUT"));

    rshport = get_rshport(testflag, rshport, "RCMD_CMD");
    
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
    FILE *fd, *fda, *in;
    char buf[MAXBUF], cbuf[MAXBUF], pipebuf[2048];
    int status, i, piping, pollret, nrofargs, arg, fdf;
    char *p, *command, **rsh, *cd, **q, *rshstring, **cmd;
    node_t *nodeptr;
    size_t maxnodelen;
    struct pollfd fds[2];

    i = 0;
    piping = 0;
    in = NULL;
    maxnodelen = 0;

    for (nodeptr = nodelink; nodeptr != NULL; nodeptr = nodeptr->next)
	if (strlen(nodeptr->name) > maxnodelen)
	    maxnodelen = strlen(nodeptr->name);

    if (debug && username != NULL)
	(void)printf("As User: %s\n", username);

    /* construct the command from the remains of argv */
    for (i=0, p=*argv, q=argv; p != NULL; p = *++q)
	i += (strlen(p)+1);
    command = (char *)calloc(i+1, sizeof(char));
    for (p = *argv; p != NULL; p = *++argv ) {
	strcat(command, p);
	strcat(command, " ");
    }
    if (debug)
	(void)printf("\nDo Command: %s\n", command);

    if (strcmp(command,"") == 0) {
	piping = 1;

	/* are we a terminal?  then go interactive! */
	if (isatty(STDIN_FILENO) && piping)
	    (void)printf("%s>", progname);
	in = fdopen(STDIN_FILENO, "r");

	/* start reading stuff from stdin and process */
	free(command);
	command = fgets(cbuf, sizeof(cbuf), in);
	if (command != NULL)
	    if (strcmp(command,"\n") == 0)
		command = NULL;
    }
    if (allrun) {
	nodeptr = check_rand();
	if (testflag && rshport > 0 && porttimeout > 0) {
	    while (!test_node_connection(rshport, porttimeout, nodeptr)) {
		    fprintf(stderr, "Skipping down node %s.\n", nodeptr->name);
		    nodeptr = check_rand();
	    }
        }
    }

    rsh = parse_rcmd("RCMD_CMD", "RCMD_CMD_ARGS", &nrofargs);
    rshstring = build_rshstring(rsh, nrofargs);
    
    /* begin the processing loop */
    while (command != NULL) {
	if (!allrun) {
		nodeptr = check_rand();
		if (testflag && rshport > 0 && porttimeout > 0) {
			while (!test_node_connection(rshport, porttimeout,
				nodeptr)) {
				fprintf(stderr, "Skipping down node %s.\n",
				    nodeptr->name);
				nodeptr = check_rand();
			}
		}
	}
	if (debug)
	    printf("Working node: %s\n", nodeptr->name);
	/* we set up pipes for each node, to prepare
	 * for the oncoming barrage of data.
	 */
	if (pipe(nodeptr->out.fds) != 0)
	    bailout();
	if (pipe(nodeptr->err.fds) != 0)
	    bailout();
	nodeptr->childpid = fork();
	/* its the ol fork and switch routine eh? */
	switch (nodeptr->childpid) {
	case -1:
	    bailout();
	    break;
	case 0: 
	    /* stupid unix tricks vol 1 */
	    if (dup2(nodeptr->out.fds[1], STDOUT_FILENO) != STDOUT_FILENO)
		bailout();
	    if (dup2(nodeptr->err.fds[1], STDERR_FILENO) != STDERR_FILENO)
		bailout();
	    if (close(nodeptr->out.fds[0]) != 0)
		bailout();
	    if (close(nodeptr->err.fds[0]) != 0)
		bailout();
	    /* stdin & stderr non-blocking */
	    fdf = fcntl(nodeptr->out.fds[0], F_GETFL);
	    fcntl(nodeptr->out.fds[0], F_SETFL, fdf|O_NONBLOCK);
	    fdf = fcntl(nodeptr->err.fds[0], F_GETFL);
	    fcntl(nodeptr->err.fds[0], F_SETFL, fdf|O_NONBLOCK);
	    
	    if (username != NULL)
		(void)snprintf(buf, MAXBUF, "%s@%s", username, nodeptr->name);
	    else
		(void)snprintf(buf, MAXBUF, "%s", nodeptr->name);
	    if (debug)
		(void)printf("%s %s %s\n", rshstring, buf, command);
	    cmd = calloc(nrofargs+1, sizeof(char *));
	    arg = 0;
	    while (rsh[arg] != NULL) {
		    cmd[arg] = rsh[arg];
		    arg++;
	    }
	    cmd[arg++] = buf;
	    cmd[arg++] = command;
	    cmd[arg] = (char *)0;
	    execvp(rsh[0], cmd);
	    bailout();
	} /* end switch */
	/* now close off the useless stuff, and read the goodies */
	if (close(nodeptr->out.fds[1]) != 0)
	    bailout();
	if (close(nodeptr->err.fds[1]) != 0)
	    bailout();
	fda = fdopen(nodeptr->out.fds[0], "r"); /* stdout */
	if (fda == NULL)
	    bailout();
	fd = fdopen(nodeptr->err.fds[0], "r"); /* stderr */
	if (fd == NULL)
	    bailout();
	fds[0].fd = nodeptr->out.fds[0];
	fds[1].fd = nodeptr->err.fds[0];
	fds[0].events = POLLIN|POLLPRI;
	fds[1].events = POLLIN|POLLPRI;
	pollret = 1;
	while (pollret >= 0) {
	    int gotdata;

	    pollret = poll(fds, 2, 5);
	    gotdata = 0;
	    if ((fds[0].revents&POLLIN) == POLLIN ||
		(fds[0].revents&POLLHUP) == POLLHUP ||
		(fds[0].revents&POLLPRI) == POLLPRI) {
		while ((cd = fgets(pipebuf, sizeof(pipebuf), fda))) {
		    (void)printf("%*s: %s", -maxnodelen,
			nodeptr->name, cd);
		    gotdata++;
		}
	    }
	    if ((fds[1].revents&POLLIN) == POLLIN ||
		(fds[1].revents&POLLHUP) == POLLHUP ||
		(fds[1].revents&POLLPRI) == POLLPRI) {
		while ((cd = fgets(pipebuf, sizeof(pipebuf), fd))) {
		    if (errorflag) 
			(void)printf("%*s: %s", -maxnodelen,
			    nodeptr->name, cd);
		    gotdata++;
		}
	    }
	    if (!gotdata)
		if (((fds[0].revents&POLLHUP) == POLLHUP ||
		     (fds[0].revents&POLLERR) == POLLERR ||
		     (fds[0].revents&POLLNVAL) == POLLNVAL) &&
		    ((fds[1].revents&POLLHUP) == POLLHUP ||
		     (fds[1].revents&POLLERR) == POLLERR ||
		     (fds[1].revents&POLLNVAL) == POLLNVAL))
		    break;
	}
	fclose(fda);
	fclose(fd);
	(void)wait(&status);

	/* yes, this is code repetition, no need to adjust your monitor */
	if (piping) {
	    if (isatty(STDIN_FILENO) && piping)
		(void)printf("%s>", progname);
	    command = fgets(cbuf, sizeof(cbuf), in);
	    if (command != NULL)
		if (strcmp(command,"\n") == 0)
		    command = NULL;
	} else
	    command = NULL;
    } /* while loop */
    if (piping) {  /* I learned this the hard way */
	fflush(in);
	fclose(in);
    } else
	    free(command);
    free(rshstring);
}
