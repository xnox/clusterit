/* $Id: common.c,v 1.1 1999/10/14 16:52:08 garbled Exp $ */
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

/*
 * these are routines that are used in all the dsh programs, and therefore
 * rather than having to fix them in n places, they are kept here.
 */

#include "common.h"

#if !defined(lint) && defined(__NetBSD__)
__COPYRIGHT(
"@(#) Copyright (c) 1998\n\
        Tim Rightnour.  All rights reserved\n");
#endif /* not lint */

#if !defined(lint) && defined(__NetBSD__)
__RCSID("$Id: common.c,v 1.1 1999/10/14 16:52:08 garbled Exp $");
#endif


/*
 * This routine just rips open the various arrays and prints out information
 * about what the command would have done, and the topology of your cluster.
 * Invoked via the -q switch.
 */

void
do_showcluster(nodelist, fanout)
	char *nodelist[];
	int fanout;
{
	int i, j, l, n;

	i = l = 0;
	for (i=0; nodelist[i] != NULL; i++)		/* just count the nodes */
		;
	j = i / fanout;
      /* how many times do I have to run in order to reach them all */
	if (i % fanout)
		j++;

	if (rungroup[0] != NULL) {
		(void)printf("Rungroup:");
		for (i=0; rungroup[i] != NULL; i++) {
			if (!(i % 4) && i > 0)
				(void)printf("\n");
			(void)printf("\t%s", rungroup[i]);
		}
		if (i % 4)
			(void)printf("\n");
	}	

	if (getenv("CLUSTER"))
		(void)printf("Cluster file: %s\n", getenv("CLUSTER"));
	(void)printf("Fanout size: %d\n", fanout);
	for (n=0; n <= j; n++) {
		for (i=n * fanout;
		    ((nodelist[i] != NULL) && (i < (n + 1) * fanout)); i++) {
			if (rungroup[0] != NULL) {
				if (test_node(i)) {
					l++;
					if (grouplist[i] == NULL)
						(void)printf("Node: %3d  Fangroup: %3d  Rungroup: None"
						    "             Host: %s\n", l, n + 1, nodelist[i]);
					else
						(void)printf("Node: %3d  Fangroup: %3d  Rungroup: "
						    "%-15s  Host: %-15s\n", l, n + 1, grouplist[i],
							nodelist[i]);
				}
			} else {
				l++;
				if (grouplist[i] == NULL)
					(void)printf("Node: %3d  Fangroup: %3d  Rungroup: None"
					    "            Host: %-15s\n", l, n + 1, nodelist[i]);
				else
					(void)printf("Node: %3d  Fangroup: %3d  Rungroup: %-15s"
					    "  Host: %-15s\n", l, n + 1, grouplist[i],
						nodelist[i]);
			}
		}
	}
}


/*
 * A routine to parse the command arguments, and prepare a nodelist for use
 */

void
parse_cluster(nodename, exclude, nodelist)
	char *nodename, **exclude, **nodelist;
{
	FILE *fd;
	char *clusterfile, *p, *group;
	int i, j, fail;
	char	buf[256];
	extern int errno;

	group = NULL;

    /* if -w wasn't specified, we need to parse the cluster file */
	clusterfile = getenv("CLUSTER");
	if (clusterfile == NULL) {
		(void)fprintf(stderr,
			"must use -w flag without CLUSTER environment setting.\n");
		exit(EXIT_FAILURE);
	}
	fd = fopen(clusterfile, "r");
	if (NULL == fd) {
		(void)fprintf(stderr, "dsh: open of clusterfile failed:%s\n",
		    strerror(errno));
		exit(EXIT_FAILURE);
	}
	i = 0;
	while ((nodename = fgets(buf, sizeof(buf), fd))) {
		p = (char *)strsep(&nodename, "\n");
		if (strcmp(p, "") != 0) {
			if (exclusion) {		/* this handles the -x option */
				fail = 0;
				for (j = 0; exclude[j] != NULL; j++)
					if (strcmp(p, exclude[j]) == 0)
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
			} /* exlusion */
		} /* strcmp */
	} /* while nodename */
	nodelist[i] = '\0';
	grouplist[i] = '\0';
	fclose(fd);
}

/* test routine, saves a ton of repetive code */

int
test_node(count)
	int count;
{
	int i;

	if (rungroup[0] == NULL)
		return(1);
	else
		if (grouplist[count] != NULL)
			for (i=0; rungroup[i] != NULL; i++)
				if (strcmp(rungroup[i], grouplist[count]) == 0)
					return(1);
	return(0);
}

/* return a string, followed by n - strlen spaces */

char *
alignstring(string, n)
	char *string;
	size_t n;
{
	size_t i;
	char *newstring;

	newstring = strdup(string);
	for (i=1; i <= n - strlen(string); i++)
		newstring = strcat(newstring, " ");

	return(newstring);
}


/* 
 * Simple error handling routine, needs severe work.
 * Its almost totally useless.
 */

/*ARGSUSED*/
void
bailout(line) 
	int line;
{
	extern int errno;
	
	if (debug)
		(void)fprintf(stderr, "Failed on line %d: %s %d\n", line,
			strerror(errno), errno);
	else
		(void)fprintf(stderr, "Internal error, aborting: %s\n",
			strerror(errno));

	_exit(EXIT_FAILURE);
}
