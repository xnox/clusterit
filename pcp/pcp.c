/* $Id: pcp.c,v 1.2 1998/10/13 07:10:46 garbled Exp $ */
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

#ifndef lint
__COPYRIGHT(
"@(#) Copyright (c) 1998\n\
        Tim Rightnour.  All rights reserved.\n");
#endif /* not lint */

#ifndef lint
__RCSID("$Id: pcp.c,v 1.2 1998/10/13 07:10:46 garbled Exp $");
#endif


#define MAX_CLUSTER 90

extern int errno;
#ifdef __NetBSD__
void do_copy __P((char **argv, char *nodelist[], int recurse, int preserve));
#else
void do_copy(char **argv, char *nodelist[], int recurse, int preserve);
char * strsep(char **stringp, const char *delim);
#endif
/* 
 *  pcp is a cluster management tool derrived from the IBM tool of the
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
	int someflag, ch, i, preserve, recurse;
	char *p, *nodelist[MAX_CLUSTER], *nodename, *clusterfile;
	char buf[256];

	someflag = 0;
	preserve = 0;
	recurse = 0;

	while ((ch = getopt(argc, argv, "?prw:")) != -1)
		switch (ch) {
		case 'p':
			preserve = 1;
			break;
		case 'r':
			recurse = 1;
			break;
		case 'w':
			someflag = 1;
			i = 0;
			for (p = optarg; p != NULL; ) {
				nodename = (char *)strsep(&p, ",");
				if (nodename != NULL)
					nodelist[i++] = strdup(nodename);
			}
			nodelist[i] = '\0';
			break;
		case '?':
			(void)fprintf(stderr, "usage: pcp [-w node1,..,nodeN] source_file [desitination_file]\n");
			exit(EXIT_FAILURE);
			break;
		default:
			break;
	}
	if (!someflag) {
		clusterfile = getenv("CLUSTER");
		if (clusterfile == NULL) {
			fprintf(stderr, "must use -w flag without CLUSTER environment setting.\n");
			exit(EXIT_FAILURE);
		}
		fd = fopen(clusterfile, "r");
		i = 0;
		while ((nodename = fgets(buf, sizeof(buf), fd))) {
			p = (char *)strsep(&nodename, "\n");
			if (strcmp(p, "") != 0)
				nodelist[i++] = (char *)strdup(p);
		}
		nodelist[i] = '\0';
		fclose(fd);
	}
	argc -= optind;
	argv += optind;
	do_copy(argv, nodelist, recurse, preserve);
}

/* 
 * Do the actual dirty work of the program, now that the arguments
 * have all been parsed out.
 */

void do_copy(argv, nodelist, recurse, preserve)
	char **argv;
	char *nodelist[];
	int recurse, preserve;
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
	if (*argv != NULL)
		source_file = strdup(*argv);
	if (*++argv != NULL) {
		destination_file = strdup(*argv);
	} else 
		destination_file = strdup(source_file);

#ifdef DEBUG
	printf ("\nDo Copy: %s %s\n", source_file, destination_file);
#endif
	for (i=0; nodelist[i] != NULL; i++) {
		rcp = getenv("RCP_CMD");
		if (rcp == NULL)
			rcp = "rcp";
		sprintf(args," ");
		if (recurse)
			strcat(args,"-r ");
		if (preserve)
			strcat(args,"-p ");
		sprintf(buf,"%s %s %s %s:%s", rcp, args, source_file, nodelist[i], destination_file);
		command = strdup(buf);
#ifdef DEBUG
		printf("%s\n", command);
#endif
		system(command);
	}
}
