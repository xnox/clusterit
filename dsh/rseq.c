/* $Id: rseq.c,v 1.21 2007/01/09 22:00:15 garbled Exp $ */
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

#include "../common/common.h"

#if !defined(lint) && defined(__NetBSD__)
__COPYRIGHT(
"@(#) Copyright (c) 1998, 1999, 2000\n\
        Tim Rightnour.  All rights reserved\n");
__RCSID("$Id: rseq.c,v 1.21 2007/01/09 22:00:15 garbled Exp $");
#endif

/* externs */
extern int errno;

void do_command(char **argv, int allrun, char *username);
node_t *check_seq(void);
void sig_handler(int i);

/* globals */

int debug, errorflag, exclusion, grouping;
int seqnumber, nrofrungroups;
int rshport, testflag, porttimeout;
char **rungroup;
char **lumplist;
node_t *nodelink;
group_t *grouplist;
char *progname;

/* 
 *  seq is a cluster management tool based upon the IBM tool dsh.
 *  Seq is used to run a command on a single node, and then run the next
 *  command on the next node in the clusterfile sequentially.
 */

int main(int argc, char **argv) 
{
    extern char *optarg;
    extern int optind;
    extern char *version;

    int someflag, ch, i, allflag, showflag;
    char *p, *group, *nodename, *username, *q;
    char **exclude, **grouptemp;

    someflag = 0;
    seqnumber = -1;
    showflag = 0;
    exclusion = 0;
    debug = 0;
    errorflag = 0;
    allflag = 0;
    testflag = 0;
    grouping = 0;
    nrofrungroups = 0;
    porttimeout = 5; /* 5 seconds to port timeout */
    username = NULL;
    nodename = NULL;
    group = NULL;

    rungroup = calloc(GROUP_MALLOC, sizeof(char **));
    if (rungroup == NULL)
	bailout();
    exclude = calloc(GROUP_MALLOC, sizeof(char **));
    if (exclude == NULL)
	bailout();

    progname = p = q = strdup(argv[0]);
    while (progname != NULL) {
	q = progname;
	progname = (char *)strsep(&p, "/");
    }
    progname = q;

#if defined(__linux__)
    while ((ch = getopt(argc, argv, "+?adeiqtg:l:o:p:w:x:")) != -1)
#else
    while ((ch = getopt(argc, argv, "?adeiqtvg:l:o:p:w:x:")) != -1)
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
	case 't':           /* test the nodes before connecting */
	    testflag = 1;
	    break;
	case 'p':           /* what is the rsh port number? */
	    rshport = atoi(optarg);
	    break;
	case 'q':		/* just show me some info and quit */
	    showflag = 1;
	    break;
	case 'o':               /* set the test timeout in seconds */
	    porttimeout = atoi(optarg);
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
	    nrofrungroups = i;
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
	case 'v':
	    (void)printf("%s: %s\n", progname, version);
	    exit(EXIT_SUCCESS);
	    break;
	case '?':		/* you blew it */
	    (void)fprintf(stderr,
	     	  "usage: %s [-aeiqv] [-g rungroup1,...,rungroupN] "
	          "[-l username] [-x node1,...,nodeN] [-w node1,..,nodeN] "
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

/* this should be atomic, but *hello* this is *userland* */

static void
test_and_set(void)
{
    int i;
    char *p, *seqfile;
    char buf[MAXBUF];
    FILE *sd;
    node_t *nodeptr, *nodex;

    p = buf;
    i = 0;

    seqfile = getenv("SEQ_FILE");
    if (seqfile == NULL) {
	(void)snprintf(buf, MAXBUF, "/tmp/%d.%s", (int)getppid(), progname);
	seqfile = strdup(buf);
    }
    sd = fopen(seqfile, "r");
    if (sd == NULL)
	sd = fopen(seqfile, "w");
    else {
	fscanf(sd, "%s", p);
	seqnumber = atoi(p);
	fclose(sd);
	sd = fopen(seqfile, "w");
    }
    if (sd == NULL)
	bailout();
    nodeptr = check_seq();
    for (nodex = nodelink; nodex != nodeptr; nodex = nodex->next)
	i++;
    (void)fprintf(sd, "%d", i);
    fclose(sd);
    seqnumber = i;
}

/* return the node number of the next node in the seqence. */

node_t *
check_seq(void)
{
    int i, g;
    node_t *nodeptr;

    if (seqnumber == -1)
	return(nodelink);

    g = seqnumber;
    i = 0;
    for (nodeptr = nodelink; (nodeptr && i < g);
	 nodeptr = nodeptr->next)
	i++;
    if (nodeptr == NULL || nodeptr->next == NULL)
	return(nodelink);
    return(nodeptr->next);
}


/* 
 * Do the actual dirty work of the program, now that the arguments
 * have all been parsed out.
 */

void 
do_command(char **argv, int allrun, char *username)
{
    struct sigaction signaler;
    FILE *fd, *fda, *in;
    char buf[MAXBUF], cbuf[MAXBUF], pipebuf[2048];
    int status, piping, pollret, i, nrofargs, arg;
    size_t maxnodelen;
    char *p, *command, **rsh, *cd, **q, **cmd, *rshstring;
    node_t *nodeptr;
    struct pollfd fds[2];

    maxnodelen = 0;
    piping = 0;
    in = NULL;

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
	(void)printf("Do Command: %s\n", command);

    for (nodeptr = nodelink; nodeptr != NULL; nodeptr = nodeptr->next)
	if (strlen(nodeptr->name) > maxnodelen)
	    maxnodelen = strlen(nodeptr->name);

    if (strcmp(command,"") == 0) {
	piping = 1;
	if (isatty(STDIN_FILENO) && piping)
	    /* are we a terminal?  then go interactive! */
	    (void)printf("%s>", progname);
	in = fdopen(STDIN_FILENO, "r");
	command = fgets(cbuf, sizeof(cbuf), in);
	/* start reading stuff from stdin and process */
	if (command != NULL)
	    if (strcmp(command,"\n") == 0)
		command = NULL;
    }

    rsh = parse_rcmd("RCMD_CMD", "RCMD_CMD_ARGS", &nrofargs);
    rshstring = build_rshstring(rsh, nrofargs);
    signaler.sa_handler = sig_handler;
    signaler.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &signaler, NULL);

    if (allrun)
	test_and_set();

    while (command != NULL) {
	if (!allrun)
	    test_and_set();
	nodeptr = check_seq();
	if (testflag && rshport > 0 && porttimeout > 0) {
	    while (!test_node_connection(rshport, porttimeout, nodeptr)) {
		    fprintf(stderr, "Skipping down node %s.\n", nodeptr->name);
		    test_and_set();
		    nodeptr = check_seq();
	    }
        }
	i = seqnumber;
	if (debug)
	    (void)printf("On node: %s\n", nodeptr->name);
	pipe(nodeptr->out.fds);
	/* we set up pipes for each node, to prepare for the
	 * oncoming barrage of data */
	pipe(nodeptr->err.fds);
	switch (fork()) {  /* its the ol fork and switch routine eh? */
	case -1:
	    bailout();
	    break;
	case 0: 
	    if (dup2(nodeptr->out.fds[1], STDOUT_FILENO) != STDOUT_FILENO)
		bailout();
	    if (dup2(nodeptr->err.fds[1], STDERR_FILENO) != STDERR_FILENO)
		bailout();
	    if (close(nodeptr->out.fds[0]) != 0)
		bailout();
	    if (close(nodeptr->err.fds[0]) != 0)
		bailout();
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
		cd = fgets(pipebuf, sizeof(pipebuf), fda);
		if (cd != NULL) {
		    (void)printf("%*s: %s",
				 -maxnodelen, nodeptr->name, cd);
		    gotdata++;
		}
	    }
	    if ((fds[1].revents&POLLIN) == POLLIN ||
		(fds[1].revents&POLLHUP) == POLLHUP ||
		(fds[1].revents&POLLPRI) == POLLPRI) {
		cd = fgets(pipebuf, sizeof(pipebuf), fd);
		if (errorflag && cd != NULL) {
		    (void)printf("%*s: %s",
				 -maxnodelen, nodeptr->name, cd);
		    gotdata++;
		} else if (!errorflag && cd != NULL)
		    gotdata++;
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
	if (piping) {
	    if (isatty(STDIN_FILENO) && piping)
	    /* yes, this is code repetition, no need to adjust your monitor */
		(void)printf("%s>", progname);
	    command = fgets(cbuf, sizeof(cbuf), in);
	    if (command != NULL)
		if (strcmp(command,"\n") == 0)
		    command = NULL;
	} else
	    command = NULL;
    } /* while loop */
    if (piping) {
	/* I learned this the hard way */
	fflush(in);
	fclose(in);
    }
}

void
sig_handler(int i)
{
    switch (i) {
    case SIGALRM:
	break;
    default:
	bailout();
	break;
    }
}
