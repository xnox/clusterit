/* $Id: rseq.c,v 1.7 1999/10/14 16:50:52 garbled Exp $ */
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
#include "common.h"

#if !defined(lint) && defined(__NetBSD__)
__COPYRIGHT(
"@(#) Copyright (c) 1998\n\
        Tim Rightnour.  All rights reserved\n");
#endif /* not lint */

#if !defined(lint) && defined(__NetBSD__)
__RCSID("$Id: rseq.c,v 1.7 1999/10/14 16:50:52 garbled Exp $");
#endif

#ifndef __P
#define __P(protos) protos
#endif

/* externs */
extern int errno;

void do_command __P((char **, char *[], int, char *));
int check_seq __P((char *[]));

/* globals */

int debug, errorflag, exclusion;
int seqnumber;
char *grouplist[MAX_CLUSTER];
char *rungroup[MAX_GROUPS];

/* 
 *  seq is a cluster management tool based upon the IBM tool dsh.
 *  It allows a user, or system administrator to issue
 *  commands in paralell on a group of machines.
 */

int main(argc, argv) 
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;

	int someflag, ch, i, allflag, showflag, exclusion;
	char *p, *group, *nodelist[MAX_CLUSTER], *nodename, *username;
	char *exclude[MAX_CLUSTER];

	extern int debug;
	extern int errorflag;

	someflag = 0;
	seqnumber = -1;
	showflag = 0;
	exclusion = 0;
	debug = 0;
	errorflag = 0;
	allflag = 0;
	username = NULL;
	nodename = NULL;
	group = NULL;
	for (i=0; i < MAX_GROUPS; i++)
		rungroup[i] = NULL;

	while ((ch = getopt(argc, argv, "?adeiqg:l:w:x:")) != -1)
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
			    "usage: seq [-aeiq] [-g rungroup1,...,rungroupN] "
				"[-l username] [-x node1,...,nodeN] [-w node1,..,nodeN] "
				"[command ...]\n");
			exit(EXIT_FAILURE);
			break;
		default:
			break;
	}
	if (!someflag)
		parse_cluster(nodename, exclude, nodelist);	
	argc -= optind;
	argv += optind;
	if (showflag) {
		do_showcluster(nodelist, DEFAULT_FANOUT);
		exit(EXIT_SUCCESS);
	}
	do_command(argv, nodelist, allflag, username);
	exit(EXIT_SUCCESS);
}

/* this should be atomic, but *hello* this is *userland* */

void
test_and_set(nodelist)
	char *nodelist[];
{
	int i;
	char *p, *seqfile;
	char buf[256];
	FILE *sd;

	p = buf;
	i = 0;

	seqfile = getenv("SEQ_FILE");
	if (seqfile == NULL) {
		(void)sprintf(buf, "/tmp/%d.seq", (int)getppid());
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
		bailout(__LINE__);
	i = check_seq(nodelist);
	if (i == -1)
		i++;
	(void)fprintf(sd, "%d", i);
	fclose(sd);
	seqnumber = i;
}

/* return the node number of the next node in the seqence. */

int 
check_seq(nodelist)
	char *nodelist[];
{
	int i, g;

	if (rungroup[0] == NULL)
		if (seqnumber == -1)
			return(0);
		else {
			g = seqnumber;
			if (nodelist[g+1] == NULL) {
				for (i=0; nodelist[i] != NULL && test_node(i) == 0; i++)
					;
				return(i);
			} else
				return(g+1);
		}
	else {
		if (seqnumber == -1) {
			for (i=0; nodelist[i] != NULL && test_node(i) == 0; i++)
				;
			return(i);
		} else {
			g = seqnumber;
			for (i=g+1; nodelist[i] != NULL && test_node(i) == 0; i++)
				;
			if (nodelist[i] == NULL)
				for (i=0; nodelist[i] != NULL && test_node(i) == 0; i++)
					;
			return(i);
		}
	} /* else */
}


/* 
 * Do the actual dirty work of the program, now that the arguments
 * have all been parsed out.
 */

void 
do_command(argv, nodelist, allrun, username)
	char **argv;
	char *nodelist[];
	char *username;
	int allrun;
{
	FILE *fd, *in;
	int out[2];
	int err[2];
	char buf[MAXBUF];
	int status, i, piping;
	char *p, *command, *rsh;

	extern int debug;

	i = 0;
	piping = 0;
	in = NULL;

	if (debug) {
		if (username != NULL)
			(void)printf("As User: %s\n", username);
	} 

	/* construct the command from the remains of argv */
	command = (char *)malloc(MAXBUF * sizeof(char));
	for (p = *argv; p != NULL; p = *++argv ) {
		strcat(command, p);
		strcat(command, " ");
	}
	if (debug) {
		(void)printf("Do Command: %s\n", command);
	}
	if (strcmp(command,"") == 0) {
		piping = 1;
		if (isatty(STDIN_FILENO) && piping)
/* are we a terminal?  then go interactive! */
			(void)printf("seq>");
		in = fdopen(STDIN_FILENO, "r");
		command = fgets(buf, sizeof(buf), in);
/* start reading stuff from stdin and process */
		if (command != NULL)
			if (strcmp(command,"\n") == 0)
				command = NULL;
	}
	if (allrun)
		test_and_set(nodelist);
	while (command != NULL) {
		if (!allrun)
			test_and_set(nodelist);
		i = seqnumber;
		if (debug)
			(void)printf("On node: %s\n", nodelist[check_seq(nodelist)]);
		pipe(out);
/* we set up pipes for each node, to prepare for the
 * oncoming barrage of data */
		pipe(err);
		switch (fork()) {  /* its the ol fork and switch routine eh? */
			case -1:
				bailout(__LINE__);
				break;
			case 0: 
				if (dup2(out[1], STDOUT_FILENO) != STDOUT_FILENO)
					bailout(__LINE__);
				if (dup2(err[1], STDERR_FILENO) != STDERR_FILENO)
					bailout(__LINE__);
				if (close(out[0]) != 0)
					bailout(__LINE__);
				if (close(err[0]) != 0)
					bailout(__LINE__);
				rsh = getenv("RCMD_CMD");
				if (rsh == NULL)
					rsh = "rsh";
				if (debug)
					printf("%s %s %s\n", rsh, nodelist[i], command);
				if (username != NULL)
/* interestingly enough, this -l thing works great with ssh */
					execlp(rsh, rsh, "-l", username, nodelist[i],
						command, (char *)0);
				else
					execlp(rsh, rsh, nodelist[i], command, (char *)0);
				bailout(__LINE__);
		} /* end switch */
		if (close(out[1]) != 0)
/* now close off the useless stuff, and read the goodies */
			bailout(__LINE__);
		if (close(err[1]) != 0)
			bailout(__LINE__);
		fd = fdopen(out[0], "r"); /* stdout */
		while ((p = fgets(buf, sizeof(buf), fd)))
			(void)printf("%s:\t%s", nodelist[i], p);
		fclose(fd);
		fd = fdopen(err[0], "r"); /* stderr */
		while ((p = fgets(buf, sizeof(buf), fd)))
			if (errorflag)
				(void)printf("%s:\t%s", nodelist[i], p);
		fclose(fd);
		(void)wait(&status);
		if (piping) {
			if (isatty(STDIN_FILENO) && piping)
/* yes, this is code repetition, no need to adjust your monitor */
				(void)printf("seq>");
			command = fgets(buf, sizeof(buf), in);
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
