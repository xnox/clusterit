/* $Id: jsh.c,v 1.7 2001/08/14 04:43:47 garbled Exp $ */
/*
 * Copyright (c) 2000
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

#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "../common/common.h"
#include "../common/sockcommon.h"

#if !defined(lint) && defined(__NetBSD__)
__COPYRIGHT(
"@(#) Copyright (c) 2000\n\
        Tim Rightnour.  All rights reserved\n");
__RCSID("$Id: jsh.c,v 1.7 2001/08/14 04:43:47 garbled Exp $");
#endif

#ifndef __P
#define __P(protos) protos
#endif

void do_command __P((char **, int, char *));
char *check_node __P((void));
void free_node __P((char *));
void log_bailout __P((int));

/* globals */

int debug, exclusion, grouping;
int errorflag, iportnum, oportnum;
char **rungroup;
char **lumplist;
char *progname, *jsd_host;
group_t *grouplist;
node_t *nodelink;

/* 
 * jsh contacts the jsd daemon, and asks for a node to work on.
 */

int
main(int argc, char *argv[])
{
	int someflag, ch, allflag, showflag;
	char *p, *q, *group, *nodename, *username;
	node_t *nodeptr;

	extern char *optarg;
	extern int optind;

	iportnum = oportnum = 0;
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
	jsd_host = NULL;

	progname = p = q = argv[0];
	while (progname != NULL) {
		q = progname;
		progname = (char *)strsep(&p, "/");
	}
	progname = strdup(q);

	while ((ch = getopt(argc, argv, "?adeil:k:p:")) != -1)
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
		case 'o':		/* port to get nodes from jsd on */
			oportnum = atoi(optarg);
			break;
		case 'p':		/* port to release nodes to jsd on */
			iportnum = atoi(optarg);
			break;
		case 'h':		/* host to connect to jsd on */
			jsd_host = strdup(optarg);
			break;			
		case '?':		/* you blew it */
			(void)fprintf(stderr,
			    "usage: %s [-aei] [-l username] [-p port] [-o port] "
				"[-h hostname] [command ...]\n", progname);
			exit(EXIT_FAILURE);
			break;
		default:
			break;
	}

	argc -= optind;
	argv += optind;

	if (!iportnum) {
		if (getenv("JSD_IPORT"))
			iportnum = atoi(getenv("JSD_IPORT"));
		else
			iportnum = JSDIPORT;
	}

	if (!oportnum) {
		if (getenv("JSD_OPORT"))
			oportnum = atoi(getenv("JSD_OPORT"));
		else
			oportnum = JSDOPORT;
	}

	if (jsd_host == NULL) {
		if (getenv("JSD_HOST"))
			jsd_host = strdup(getenv("JSD_HOST"));
		else
			jsd_host = strdup("localhost");
	}
	if (jsd_host == NULL)
		bailout(__LINE__);

	do_command(argv, allflag, username);
	exit(EXIT_SUCCESS);
}

char *
check_node()
{
	char *buf;
	int sock, i;
	struct sockaddr_in name;
	struct hostent *hostinfo;

	/* create socket */
	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0)
		bailout(__LINE__);

	name.sin_family = AF_INET;
	name.sin_port = htons(oportnum);
	hostinfo = gethostbyname(jsd_host);

	if (hostinfo == NULL) {
		(void)fprintf(stderr, "Unknown host %s.\n", jsd_host);
		exit(EXIT_FAILURE);
	}

	name.sin_addr = *(struct in_addr *)hostinfo->h_addr;

	if (connect(sock, (struct sockaddr *)&name, sizeof(name)) < 0)
		bailout(__LINE__);

	i = read_from_client(sock, &buf); /* get a node */
	buf[i] = '\0';
    if (debug)
		printf("Got node %s\n", buf);
	(void)close(sock);

	return(buf);
}

void
free_node(nodename)
	char *nodename;
{
	int sock;
	char *buf;
	struct sockaddr_in name;
	struct hostent *hostinfo;

	/* create socket */
	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0)
		bailout(__LINE__);

	name.sin_family = AF_INET;
	name.sin_port = htons(iportnum);
	hostinfo = gethostbyname(jsd_host);

	if (hostinfo == NULL) {
		(void)fprintf(stderr, "Unknown host %s.\n", jsd_host);
		exit(EXIT_FAILURE);
	}

	name.sin_addr = *(struct in_addr *)hostinfo->h_addr;

	if (connect(sock, (struct sockaddr *)&name, sizeof(name)) < 0)
		bailout(__LINE__);

    if (debug)
		printf("Freeing node %s\n", nodename);
	read_from_client(sock, &buf);
	if (write_to_client(sock, nodename) != 0)
		bailout(__LINE__);
	if (debug)
		printf("freed node %s\n", nodename);
	(void)close(sock);
}

/* 
 * Do the actual dirty work of the program, now that the arguments
 * have all been parsed out.
 */

void
do_command(argv, allrun, username)
	char **argv;
	char *username;
	int allrun;
{
	FILE *fd, *in;
	char buf[MAXBUF];
	char *nodename;
	int status, i, piping;
	char *p, *command, *rsh;
	pipe_t out, err;
	pid_t childpid;

	i = 0;
	piping = 0;
	in = NULL;
	nodename = NULL;

	if (debug)
		if (username != NULL)
			(void)printf("As User: %s\n", username);

	/* construct the command from the remains of argv */
	command = (char *)malloc(MAXBUF * sizeof(char));
	memcpy(command, "\0", MAXBUF * sizeof(char));
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
	} else {
		close(STDIN_FILENO);
		if (open("/dev/null", O_RDONLY, NULL) != 0)
			bailout(__LINE__);
	}
	if (allrun)
		nodename = check_node();
	while (command != NULL) {
		if (!allrun)
			nodename = check_node();
		if (debug)
			printf("Working node: %s\n", nodename);
		if (pipe(out.fds) != 0)
			bailout(__LINE__);
		if (pipe(err.fds) != 0)
			bailout(__LINE__);
/* we set up pipes for each node, to prepare
 * for the oncoming barrage of data.
 */
		childpid = fork();
		switch (childpid) {
/* its the ol fork and switch routine eh? */
			case -1:
				bailout(__LINE__);
				break;
			case 0:
				if (piping)
					close(STDIN_FILENO);
				if (dup2(out.fds[1], STDOUT_FILENO) != STDOUT_FILENO)
/* stupid unix tricks vol 1 */
					bailout(__LINE__);
				if (dup2(err.fds[1], STDERR_FILENO) != STDERR_FILENO)
					bailout(__LINE__);
				if (close(out.fds[0]) != 0)
					bailout(__LINE__);
				if (close(err.fds[0]) != 0)
					bailout(__LINE__);
				rsh = getenv("RCMD_CMD");
				if (rsh == NULL)
					rsh = strdup("rsh");
				if (rsh == NULL)
					bailout(__LINE__);
				if (debug)
					printf("%s %s %s\n", rsh, nodename, command);
				if (username != NULL)
/* interestingly enough, this -l thing works great with ssh */
					execlp(rsh, rsh, "-l", username, nodename,
						command, (char *)0);
				else
					execlp(rsh, rsh, nodename, command, (char *)0);
				bailout(__LINE__);
		} /* end switch */
		if (close(out.fds[1]) != 0)
/* now close off the useless stuff, and read the goodies */
			bailout(__LINE__);
		if (close(err.fds[1]) != 0)
			bailout(__LINE__);
		fd = fdopen(out.fds[0], "r"); /* stdout */
		while ((p = fgets(buf, sizeof(buf), fd)))
			(void)printf("%s: %s", nodename, p);
		fclose(fd);
		fd = fdopen(err.fds[0], "r"); /* stderr */
		while ((p = fgets(buf, sizeof(buf), fd)))
			if (errorflag)
				(void)printf("%s: %s", nodename, p);
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
		if (!allrun)
			free_node(nodename);
	} /* while loop */
	if (allrun)
		free_node(nodename);
	if (piping) {  /* I learned this the hard way */
		fflush(in);
		fclose(in);
	}
}

void
log_bailout(line)
{
	bailout(line);
}
