/* $Id: pcp.c,v 1.22 2007/01/23 17:56:31 garbled Exp $ */
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
#include <poll.h>
#include <libgen.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include "../common/common.h"

#if !defined(lint) && defined(__NetBSD__)
__COPYRIGHT(
"@(#) Copyright (c) 1998, 1999, 2000\n\
        Tim Rightnour.  All rights reserved.\n");
__RCSID("$Id: pcp.c,v 1.22 2007/01/23 17:56:31 garbled Exp $");
#endif

extern int errno;

void do_copy(char **argv, int recurse, int preserve, char *username);
void paralell_copy(char *rcp, int nrof, char *username, char *source_file,
    char *destination_file);
void serial_copy(char *rcp, char *username, char *source_file,
    char *destination_file);

char **lumplist;
char **rungroup;
char *progname;
int fanout, concurrent, quiet, debug, grouping, exclusion, nrofrungroups;
int testflag, rshport, porttimeout;
node_t *nodelink;
group_t *grouplist;
volatile sig_atomic_t alarmtime;

/* 
 *  pcp is a cluster management tool based on the IBM tool of the
 *  same name.  It allows a user, or system administrator to copy files
 *  to a cluster of machines with a single command.
 */

int
main(int argc, char **argv)
{
    extern char *optarg;
    extern int optind;
    extern char *version;

    int someflag, ch, i, preserve, recurse;
    char *p, *nodename, *username, *group;
    char **exclude;
    node_t *nodeptr;

    fanout = 0;
    quiet = 1;
    concurrent = 0;
    someflag = 0;
    preserve = 0;
    recurse = 0;
    exclusion = 0;
    grouping = 0;
    nrofrungroups = 0;
    testflag = 0;
    rshport = 0;
    porttimeout = 5; /* 5 seconds to port timeout */
    username = NULL;
    group = NULL;
    nodeptr = NULL;
    nodelink = NULL;
    exclude = NULL;

    rungroup = calloc(GROUP_MALLOC, sizeof(char **));
    if (rungroup == NULL)
	bailout();

    progname = strdup(basename(argv[0]));

#if defined(__linux__)
    while ((ch = getopt(argc, argv, "+?cdeprtvf:g:l:n:o:w:x:")) != -1)
#else
    while ((ch = getopt(argc, argv, "?cdeprtvf:g:l:n:o:w:x:")) != -1)
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
	case 't':           /* test the nodes before connecting */
	    testflag = 1;
	    break;
	case 'n':           /* what is the rsh port number? */
	    rshport = atoi(optarg);
	    break;
	case 'o':               /* set the test timeout in seconds */
	    porttimeout = atoi(optarg);
	    break;
	case 'l':               /* invoke me as some other user */
	    username = strdup(optarg);
	    break;
	case 'f':		/* set the fanout size */
	    fanout = atoi(optarg);
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
	case '?':
	    (void)fprintf(stderr,
		"usage: %s [-ceprv] [-f fanout] [-g rungroup1,...,rungroupN] "
		"[-l username] [-x node1,...,nodeN] [-w node1,..,nodeN] "
		"source_file1 [source_file2 ... source_fileN] "
		"[desitination_file]\n", progname);
	    return(EXIT_FAILURE);
	    /* NOTREACHED */
	    break;
	default:
	    break;
	}
    if (fanout == 0 && getenv("FANOUT"))
	    fanout = atoi(getenv("FANOUT"));
    else
	    fanout = DEFAULT_FANOUT;
    if (username == NULL && getenv("RCP_USER"))
	    username = strdup(getenv("RCP_USER"));
    if (!rshport && getenv("RCP_PORT"))
	    rshport = atoi(getenv("RCP_PORT"));
    if (!testflag && getenv("RCMD_TEST"))
	    testflag = 1;
    if (porttimeout == 5 && getenv("RCMD_TEST_TIMEOUT"))
	    porttimeout = atoi(getenv("RCMD_TEST_TIMEOUT"));

    rshport = get_rshport(testflag, rshport, "RCP_CMD");
    
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

void do_copy(char **argv, int recurse, int preserve, char *username)
{
    int numsource, j, nrofargs;
    char *source_file, *destination_file, *tempstore, **rcp, *rcpstring;
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
		bailout();
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

    rcp = parse_rcmd("RCP_CMD", "RCP_CMD_ARGS", &nrofargs);
    rcpstring = build_rshstring(rcp, nrofargs);
    if (recurse) {
	    strcat(rcpstring, " -r ");
	    nrofargs++;
    }
    if (preserve) {
	    strcat(rcpstring, " -p ");
	    nrofargs++;
    }
        
    if (concurrent)
	    paralell_copy(rcpstring, nrofargs + numsource, username,
		source_file, destination_file);
    else
	    serial_copy(rcpstring, username, source_file, destination_file);
}

/* Copy files in paralell.  This is preferred with smaller files, because
   the initial connection and authentication latency is longer than the
   file transfer.  With large files, you generate more collisions than
   good packets, and actually slow it down, thus serial is faster. */

void
paralell_copy(char *rcp, int nrof, char *username, char *source_file,
	      char *destination_file)
{
    int i, j, n, g, status, pollret, fdf;
    char *rcpstring, pipebuf[2048], *cd;
    FILE *fd, *fda, *in;
    char **argz, **aps;
    node_t *nodeptr, *nodehold;
    size_t maxnodelen, rcplen;
    pid_t currentchild;
    struct pollfd fds[2];

    j = i = maxnodelen = 0;
    in = NULL;
    cd = pipebuf;

    argz = calloc(nrof + 3, sizeof(char *));
    if (argz == NULL)
	    bailout();
    
    for (nodeptr = nodelink; nodeptr; nodeptr = nodeptr->next) {
	if (strlen(nodeptr->name) > maxnodelen)
	    maxnodelen = strlen(nodeptr->name);
	j++;
    }

    if (username)
	    rcplen = strlen(username) + 128;
    else
	    rcplen = 128;
    rcplen += strlen(source_file) + strlen(rcp) + maxnodelen +
	strlen(destination_file);
    rcpstring = calloc(rcplen, sizeof(char));
    if (rcpstring == NULL)
	    bailout();
    
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
	     * we set up pipes for each node, to prepare for the oncoming
	     * barrage of data. Close on exec must be set here, otherwise
	     * children spawned after other children, inherit the open file
	     * descriptors, and cause the pipes to remain  open forever.
	     */
	    if (pipe(nodeptr->out.fds) != 0)
		bailout();
	    if (pipe(nodeptr->err.fds) != 0)
		bailout();
	    if (fcntl(nodeptr->out.fds[0], F_SETFD, 1) == -1)
		bailout();
	    if (fcntl(nodeptr->out.fds[1], F_SETFD, 1) == -1)
		bailout();
	    if (fcntl(nodeptr->err.fds[0], F_SETFD, 1) == -1)
		bailout();
	    if (fcntl(nodeptr->err.fds[1], F_SETFD, 1) == -1)
		bailout();
	    if (username != NULL)
		(void)snprintf(rcpstring, rcplen, "%s %s %s@%s:%s", rcp,
			       source_file, username, nodeptr->name,
			       destination_file);
	    else
		(void)snprintf(rcpstring, rcplen, "%s %s %s:%s", rcp,
			       source_file, nodeptr->name, destination_file);
	    if (debug)
		printf("Running command: %s\n", rcpstring);
	    nodeptr->childpid = fork();
	    switch (nodeptr->childpid) {
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
		/* stdin & stderr non-blocking */
		fdf = fcntl(nodeptr->out.fds[0], F_GETFL);
		fcntl(nodeptr->out.fds[0], F_SETFL, fdf|O_NONBLOCK);
		fdf = fcntl(nodeptr->err.fds[0], F_GETFL);
		fcntl(nodeptr->err.fds[0], F_SETFL, fdf|O_NONBLOCK);
		
		if (testflag && rshport > 0 && porttimeout > 0)
			if (!test_node_connection(rshport, porttimeout,
						  nodeptr))
			    exit(EXIT_SUCCESS);
		for (aps = argz; (*aps = strsep(&rcpstring, " ")) != NULL;)
		    if (**aps != '\0')
			++aps;
		execvp(argz[0], argz);
		bailout();
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
			if (!quiet)
			    (void)printf("%*s: %s", -maxnodelen,
				nodeptr->name, cd);
			gotdata++;
		    }
		}
		if ((fds[1].revents&POLLIN) == POLLIN ||
		    (fds[1].revents&POLLHUP) == POLLHUP ||
		    (fds[1].revents&POLLPRI) == POLLPRI) {
		    while ((cd = fgets(pipebuf, sizeof(pipebuf), fd))) {
			if (!quiet)
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
	    nodeptr = nodeptr->next;
	} /* pipe read */
    } /* for n */	
}

/* serial copy */

void
serial_copy(char *rcp, char *username, char *source_file,
	    char *destination_file)
{
	node_t *nodeptr;
	char *command;
	size_t maxnodelen, rcplen;

	maxnodelen = 0;

	for (nodeptr = nodelink; nodeptr; nodeptr = nodeptr->next) {
		if (strlen(nodeptr->name) > maxnodelen)
			maxnodelen = strlen(nodeptr->name);
	}
	if (username)
		rcplen = strlen(username) + 128;
	else
		rcplen = 128;
	rcplen += strlen(source_file) + strlen(rcp) + maxnodelen +
	    strlen(destination_file);
	command = calloc(rcplen, sizeof(char));
	if (command == NULL)
		bailout();
    
	for (nodeptr=nodelink; nodeptr != NULL; nodeptr = nodeptr->next) {
		if (testflag && rshport > 0 && porttimeout > 0)
			if (!test_node_connection(rshport, porttimeout,
				nodeptr))
			    continue;
		if (username != NULL)
			(void)snprintf(command, rcplen, "%s %s %s@%s:%s", rcp,
			    source_file, username, nodeptr->name,
			    destination_file);
		else
			(void)snprintf(command, rcplen, "%s %s %s:%s", rcp,
			    source_file, nodeptr->name, destination_file);
		if (debug)
			printf("Running command: %s\n", command);
		system(command);
	}
}
