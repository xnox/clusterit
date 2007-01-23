/* $Id: dsh.c,v 1.35 2007/01/23 17:56:31 garbled Exp $ */
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
#include <libgen.h>

#include "../common/common.h"

#if !defined(lint) && defined(__NetBSD__)
__COPYRIGHT(
"@(#) Copyright (c) 1998, 1999, 2000\n\
        Tim Rightnour.  All rights reserved\n");
__RCSID("$Id: dsh.c,v 1.35 2007/01/23 17:56:31 garbled Exp $");
#endif /* not lint */

void do_command(char **argv, int fanout, char *username);
void sig_handler(int i);

/* globals */
int debug, errorflag, exclusion, grouping;
int testflag, rshport, porttimeout, script;
node_t *nodelink;
group_t *grouplist;
char **rungroup;
int nrofrungroups;
char **lumplist;
pid_t currentchild;
char *progname;
char *scriptname;
volatile sig_atomic_t alarmtime, gotsigterm;

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
    extern char *version;

    int someflag, ch, fanout, showflag, fanflag;
    char *p, *group, *nodename, *username;
    char **exclude;
    struct rlimit limit;
    node_t *nodeptr;

    someflag = showflag = fanflag = 0;
    exclusion = debug = errorflag = 0;
    testflag = rshport = nrofrungroups = 0;
    porttimeout = 5; /* 5 seconds to port timeout */
    gotsigterm = grouping = 0;
    fanout = DEFAULT_FANOUT;
    nodename = NULL;
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
    while ((ch = getopt(argc, argv, "+?deiqtf:g:l:o:p:s:vw:x:")) != -1)
#else
    while ((ch = getopt(argc, argv, "?deiqtf:g:l:o:p:s:vw:x:")) != -1)
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
	case 'o':               /* set the test timeout in seconds */
	    porttimeout = atoi(optarg);
	    break;
	case 's':	/* we want to push a script and run it */
	    script = 1;
	    scriptname = strdup(optarg);
	    break;
	case 'f':		/* set the fanout size */
	    fanout = atoi(optarg);
	    fanflag = 1;
	    break;
	case 'g':		/* pick a group to run on */
	    grouping = 1;
	    nrofrungroups = parse_gopt(optarg);
	    break;
	case 'v':
	    (void)printf("%s: %s\n", progname, version);
	    exit(EXIT_SUCCESS);
	    break;
	case 'x':		/* exclude nodes, w overrides this */
	    exclusion = 1;
	    exclude = parse_xopt(optarg);
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
	        "usage:\n%s [-eiqtv] [-f fanout] [-p portnum] [-o timeout] "
		"[-g rungroup1,...,rungroupN]\n"
		"    [-l username] [-x node1,...,nodeN] [-w node1,..,nodeN] "
		"[command ...]\n", progname);
	    (void)fprintf(stderr,
		"%s [-eiqtv] [-f fanout] [-p portnum] [-o timeout] "
		"[-g rungroup1,...,rungroupN]\n"
		"    [-l username] [-x node1,...,nodeN] [-w node1,..,nodeN]\n"
	        "    -s scriptname [arguments ...]\n", progname);
	    return(EXIT_FAILURE);
	    /*NOTREACHED*/
	    break;
	default:
	    break;
	}

    /* Check for various environment variables and use them if they exist */
    if (!fanflag && getenv("FANOUT"))
        fanout = atoi(getenv("FANOUT"));
    if (!rshport && getenv("RCMD_PORT"))
	rshport = atoi(getenv("RCMD_PORT"));
    if (!testflag && getenv("RCMD_TEST"))
	testflag = 1;
    if (porttimeout == 5 && getenv("RCMD_TEST_TIMEOUT"))
	porttimeout = atoi(getenv("RCMD_TEST_TIMEOUT"));
    if (username == NULL && getenv("RCMD_USER"))
	username = strdup(getenv("RCMD_USER"));

    rshport = get_rshport(testflag, rshport, "RCMD_CMD");

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
    char buf[MAXBUF], cbuf[MAXBUF], pipebuf[2048];
    int status, i, j, n, g, piping, pollret, nrofargs, arg, slen, fdf;
    size_t maxnodelen;
    char *p, **q, *command, **rsh, *cd, **cmd, *rshstring;
    char *scriptbase, *scriptdir;
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
    for (i=0, p=*argv, q=argv; p != NULL; p = *++q)
	i += (strlen(p)+1);
    command = (char *)calloc(i+1, sizeof(char));
    for (p = *argv; p != NULL; p = *++argv ) {
	strcat(command, p);
	strcat(command, " ");
    }
    if (debug) {
	(void)printf("\nDo Command: %s\n", command);
	(void)printf("Fanout: %d Groups:%d\n", fanout, j);
    }
    if (strcmp(command, "") == 0 && !script) {
	piping = 1;
	/* are we a terminal?  then go interactive! */
	if (isatty(STDIN_FILENO) && piping)
	    (void)printf("%s>", progname);
	in = fdopen(STDIN_FILENO, "r");
	/* start reading stuff from stdin and process */
	command = fgets(cbuf, sizeof(cbuf), in);
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

    rsh = parse_rcmd("RCMD_CMD", "RCMD_CMD_ARGS", &nrofargs);
    rshstring = build_rshstring(rsh, nrofargs);
    
    /* begin the processing loop */
    g = 0;
    nodeptr = nodelink;
    while (command != NULL) {
	for (n=0; n <= j; n++) {
	    if (gotsigterm)
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
#ifndef linux
		    /* Linux needs stdin open so it gets poll() hangup events*/
		    if (piping)
			if (close(STDIN_FILENO) != 0)
			    bailout();
#endif
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
		    /* stdin & stderr non-blocking */
		    fdf = fcntl(nodeptr->out.fds[0], F_GETFL);
		    fcntl(nodeptr->out.fds[0], F_SETFL, fdf|O_NONBLOCK);
		    fdf = fcntl(nodeptr->err.fds[0], F_GETFL);
		    fcntl(nodeptr->err.fds[0], F_SETFL, fdf|O_NONBLOCK);
		    
		    if (username != NULL)
			(void)snprintf(buf, MAXBUF, "%s@%s", username,
				       nodeptr->name);
		    else
			(void)snprintf(buf, MAXBUF, "%s", nodeptr->name);
		    if (debug)
			    (void)printf("%s %s %s\n", rshstring,
				buf, command);
		    if (testflag && rshport > 0 && porttimeout > 0)
			if (!test_node_connection(rshport, porttimeout,
						  nodeptr))
			    exit(EXIT_SUCCESS);
		    /* handle the script argument */
		    if (script) {
			    scriptbase = basename(scriptname);
			    scriptdir = dirname(scriptname);
			    slen = strlen(rshstring) + strlen(scriptbase)*2 +
				strlen(buf) + strlen(command) + 128;

			    cmd = calloc(4, sizeof(char *));
			    arg = 0;
			    cmd[arg++] = "/bin/sh";
			    cmd[arg++] = "-c";
			    cmd[arg] = (char *)malloc(sizeof(char) * slen);
			    (void)snprintf(cmd[arg], slen,
				"tar cfp - %s | %s %s "
				"'mkdir /tmp/dsh.$$; cd /tmp/dsh.$$ ; "
				"tar xfp - ; ./%s %s; cd /tmp ; "
				"rm -rf /tmp/dsh.$$'", scriptbase,
				rshstring, buf, scriptbase, command);
			    if (debug)
				    (void)printf("%s\n", cmd[arg]);
			    arg++;
			    cmd[arg] = (char *)0;
			    chdir(scriptdir); /* for tar */
			    execvp("/bin/sh", cmd);
			    bailout();
		    }
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
			(fds[0].revents&POLLHUP) == POLLHUP ||
		        (fds[0].revents&POLLPRI) == POLLPRI) {
			while ((cd = fgets(pipebuf, sizeof(pipebuf), fda))) {
			    (void)printf("%*s: %s", -maxnodelen,
				nodeptr->name, cd);
			    gotdata++;
			}
		    }
		    if ((fds[1].revents&POLLIN) == POLLIN ||
			(fds[0].revents&POLLHUP) == POLLHUP ||
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
		nodeptr = nodeptr->next;
	    } /* for pipe read */			
	} /* for n */
	if (piping) {
	    /* yes, this is code repetition, no need to adjust your monitor */
	    if (isatty(STDIN_FILENO) && piping)
		(void)printf("%s>", progname);
	    command = fgets(cbuf, sizeof(cbuf), in);
	    if (command != NULL)
		if (strcmp(command,"\n") == 0)
		    command = NULL;
	    nodeptr = nodelink;
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
    default:
	bailout();
	break;
    }
}
