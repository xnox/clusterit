/* $Id: pcp.c,v 1.3 1998/10/20 07:32:58 garbled Exp $ */
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
__RCSID("$Id: pcp.c,v 1.3 1998/10/20 07:32:58 garbled Exp $");
#endif

#define MAX_CLUSTER 512
#define DEFAULT_FANOUT 64
#define MAX_GROUPS 32

extern int errno;
#ifdef __NetBSD__
void do_copy __P((char **argv, char *nodelist[], int recurse, int preserve, char *username));
int test_node __P((int count));
#else
void do_copy(char **argv, char *nodelist[], int recurse, int preserve, char *username);
char * strsep(char **stringp, const char *delim);
int test_node(int count);
#endif

char *grouplist[MAX_CLUSTER];
char *rungroup[MAX_GROUPS];

/* 
 *  pcp is a cluster management tool based on the IBM tool of the
 *  same name.  It allows a user, or system administrator to copy files
 *  to a cluster of machines with a single command.
 */

void main(argc, argv) 
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

	someflag = 0;
	preserve = 0;
	recurse = 0;
	exclusion = 0;
	username = NULL;
	group = NULL;
	for (i=0; i < MAX_GROUPS; i++)
		rungroup[i] = NULL;

	while ((ch = getopt(argc, argv, "?prg:l:w:x:")) != -1)
		switch (ch) {
		case 'p':		/* preserve file modes */
			preserve = 1;
			break;
		case 'r':		/* recursive directory operations */
			recurse = 1;
			break;
		case 'l':               /* invoke me as some other user */
			username = strdup(optarg);
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
			(void)fprintf(stderr, "usage: pcp [-pr] [-g rungroup1,...,rungroupN] [-l username] [-x node1,...,nodeN] [-w node1,..,nodeN] source_file [desitination_file]\n");
			exit(EXIT_FAILURE);
			break;
		default:
			break;
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
			if (strcmp(p, "") != 0)
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
		nodelist[i] = '\0';
		grouplist[i] = '\0';
		fclose(fd);
	}
	argc -= optind;
	argv += optind;
	do_copy(argv, nodelist, recurse, preserve, username);
	exit(EXIT_SUCCESS);
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
	char buf[256];
	char args[8];
	int i;
	char *command, *source_file, *destination_file, *rcp;

#ifdef DEBUG
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
	printf ("\nDo Copy: %s %s\n", source_file, destination_file);
#endif
	for (i=0; nodelist[i] != NULL; i++) 
		if (test_node(i)) {
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
			}
			(void)sprintf(buf,"%s %s %s %s:%s", rcp, args, source_file, nodelist[i], destination_file);
			command = strdup(buf);
#ifdef DEBUG
			printf("%s\n", command);
#endif
			system(command);
		}
}

/* test routine, saves a ton of repetive code */

int test_node(int count)
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
