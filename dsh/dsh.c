/* $Id: dsh.c,v 1.21 2005/06/02 17:04:09 garbled Exp $ */
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
#include <poll.h>

#include "../common/common.h"

#if !defined(lint) && defined(__NetBSD__)
__COPYRIGHT(
"@(#) Copyright (c) 1998, 1999, 2000\n\
        Tim Rightnour.  All rights reserved\n");
__RCSID("$Id: dsh.c,v 1.21 2005/06/02 17:04:09 garbled Exp $");
#endif /* not lint */

void do_command(char **argv, int fanout, char *username);
void sig_handler(int i);

/* globals */
int debug, errorflag, gotsigint, gotsigterm, exclusion, grouping;
int testflag, rshport, porttimeout;
node_t *nodelink;
group_t *grouplist;
char **rungroup;
int nrofrungroups;
char **lumplist;
pid_t currentchild;
char *progname;

/* 
 *  dsh is a cluster management tool based upon the IBM tool of the
 *  same name.  It allows a user, or system administrator to issue
 *  commands in paralell on a group of machines.
 */

int
main(int argc, char *argv[])
{
    extern char	*optarg;
    extern int	optind;

    int someflag, ch, i, fanout, showflag, fanflag;
    char *p, *q, *group, *nodename, *username;
    char **exclude, **grouptemp;
    struct rlimit limit;
    node_t *nodeptr;

    someflag = showflag = fanflag = 0;
    exclusion = debug = errorflag = 0;
    testflag = rshport = nrofrungroups = 0;
    porttimeout = 5; /* 5 seconds to port timeout */
    gotsigint = gotsigterm = grouping = 0;
    fanout = DEFAULT_FANOUT;
    nodename = NULL;
    username = NULL;
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
#if defined(__linux__)
    while ((ch = getopt(argc, argv, "+?deiqtf:g:l:o:p:w:x:")) != -1)
#else
    while ((ch = getopt(argc, argv, "?deiqtf:g:l:o:p:w:x:")) != -1)
#endif
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
	case 't':           /* test the nodes before connecting */
	    testflag = 1;
	    break;
	case 'p':           /* what is the rsh port number? */
	    rshport = atoi(optarg);
	    break;
	case 'f':		/* set the fanout size */
	    fanout = atoi(optarg);
	    fanflag = 1;
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
	    for (p = optarg; p != NULL; ) {
		nodename = (char *)strsep(&p, ",");
		if (nodename != NULL)
		    (void)nodealloc(nodename);
	    }
	    break;
	case '?':		/* you blew it */
	    (void)fprintf(stderr,
	        "usage: %s [-eiqt] [-f fanout] [-p portnum] [-o timeout]"
		"[-g rungroup1,...,rungroupN] [-l username] "
		"[-x node1,...,nodeN] [-w node1,..,nodeN] "
		"[command ...]\n", progname);
	    return(EXIT_FAILURE);
	    /*NOTREACHED*/
	    break;
	default:
	    break;
	}

    /* Check for various environment variables and use them if they exist */
    if (!fanflag)
	if (getenv("FANOUT"))
	    fanout = atoi(getenv("FANOUT"));
    if (!rshport)
	if (getenv("RCMD_PORT"))
	    rshport = atoi(getenv("RCMD_PORT"));
    if (!testflag)
	if (getenv("RCMD_TEST"))
	    testflag = 1;
    if (porttimeout == 5)
	if (getenv("RCMD_TEST_TIMEOUT"))
	    porttimeout = atoi(getenv("RCMD_TEST_TIMEOUT"));
    if (username == NULL)
	if (getenv("RCMD_USER"))
	    username = strdup(getenv("RCMD_USER"));

    /* we need to find or guess the port number */
    if (testflag && rshport == 0) {
	if (!getenv("RCMD_CMD"))
	    rshport = 514;
	else if (strcmp("ssh", getenv("RCMD_CMD")) == 0)
	    rshport = 22;
	else {
	    (void)fprintf(stderr, "-t argument given, but port number to test "
			  "could not be guessed.  Please set RSHPORT "
			  "environment variable to the portnumber of the "
			  "protocol you are using, or supply it with the "
			  "-p argument to dsh.");
	    return(EXIT_FAILURE);
	}
	if (debug)
	    printf("Test port: %d\n", rshport);
    }

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
	bailout();
    if (limit.rlim_cur < fanout * 5) {
	limit.rlim_cur = fanout * 5;
	if (setrlimit(RLIMIT_NOFILE, &limit) != 0)
	    bailout();
    }

    do_command(argv, fanout, username);
    return(EXIT_SUCCESS);
}



/* 
 * Do the actual dirty work of the program, now that the arguments
 * have all been parsed out.
 */

void
do_command(char **argv, int fanout, char *username)
{
    struct sigaction signaler;

    FILE *fd, *fda, *in;
    char buf[MAXBUF];
    char pipebuf[2048];
    int status, i, j, n, g, piping, pollret;
    size_t maxnodelen;
    char *p, *command, *rsh, *cd;
    node_t *nodeptr, *nodehold;
    struct pollfd fds[2];

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
    memset(command, 0, MAXBUF * sizeof(char));
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
	if (open("/dev/null", O_RDONLY, NULL) != 0)
	    bailout();
    }

    signaler.sa_handler = sig_handler;
    signaler.sa_flags = SA_RESTART;
    sigaction(SIGTERM, &signaler, NULL);
    sigaction(SIGINT, &signaler, NULL);
    sigaction(SIGALRM, &signaler, NULL);

    /* gather the rsh data */
    rsh = getenv("RCMD_CMD");
    if (rsh == NULL)
	rsh = strdup("rsh");
    if (rsh == NULL)
	bailout();

    /* begin the processing loop */
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
		nodeptr->childpid = fork();
		switch (nodeptr->childpid) {
		    /* its the ol fork and switch routine eh? */
		case -1:
		    bailout();
		    break;
		case 0:
		    /* remove from parent group to avoid kernel
		     * passing signals to children.
		     */
		    (void)setsid();
		    if (piping)
			if (close(STDIN_FILENO) != 0)
			    bailout();
		    if (dup2(nodeptr->out.fds[1], STDOUT_FILENO) 
			!= STDOUT_FILENO) 
			bailout();
		    if (dup2(nodeptr->err.fds[1], STDERR_FILENO)
			!= STDERR_FILENO)
			bailout();
		    if (close(nodeptr->out.fds[0]) != 0)
			bailout();
		    if (close(nodeptr->err.fds[0]) != 0)
			bailout();
		    if (username != NULL)
			(void)sprintf(buf, "%s@%s", username, nodeptr->name);
		    else
			(void)sprintf(buf, "%s", nodeptr->name);
		    if (debug)
			(void)printf("%s %s %s\n", rsh, buf, command);
		    if (testflag && rshport > 0 && porttimeout > 0)
			if (!test_node_connection(rshport, porttimeout,
						  nodeptr))
			    exit(EXIT_SUCCESS);
		    execlp(rsh, rsh, buf, command, (char *)0);
		    bailout();
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
				 " fanout part: %d\n", ((n) ? g-fanout+i : i),
				 n, i);
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
		        (fds[0].revents&POLLPRI) == POLLPRI) {
		    	cd = fgets(pipebuf, sizeof(pipebuf), fda);
		    	if (cd != NULL) {
			    (void)printf("%*s: %s",
			                 -maxnodelen, nodeptr->name, cd);
			    gotdata++;
			}
		    }
		    if ((fds[1].revents&POLLIN) == POLLIN ||
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
sig_handler(int i)
{
    switch (i) {
    case SIGINT:
	killpg(currentchild, SIGINT);
	break;
    case SIGTERM:
	gotsigterm = 1;
	break;
    case SIGALRM:
	break;
    default:
	bailout();
	break;
    }
}
