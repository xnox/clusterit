/* $Id: common.c,v 1.34 2007/04/02 18:49:07 garbled Exp $ */
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

/*
 * these are routines that are used in all the dsh programs, and therefore
 * rather than having to fix them in n places, they are kept here.
 */

#include "common.h"

#if !defined(lint) && defined(__NetBSD__)
__COPYRIGHT(
"@(#) Copyright (c) 1998, 1999, 2000\n\
        Tim Rightnour.  All rights reserved\n");
__RCSID("$Id: common.c,v 1.34 2007/04/02 18:49:07 garbled Exp $");
#endif

char *version = "ClusterIt Version 2.4.1_BETA";

#ifdef CLUSTERS

/* convenience: unified -g parsing */

int
parse_gopt(char *oa)
{
	int i;
	char **grouptemp;
	char *group, *p;
	
	i = 0;
	for (p = oa; p != NULL; ) {
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
	return(i);
}

/* convenience: unified -x parsing */

char **
parse_xopt(char *oa)
{
	int i;
	char **grouptemp, **exclude;
	char *nodename, *p;

	exclude = calloc(GROUP_MALLOC, sizeof(char **));
	if (exclude == NULL)
		bailout();
	i = 0;
	for (p = oa; p != NULL; ) {
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
	return(exclude);
}

/*
 * This routine just rips open the various arrays and prints out information
 * about what the command would have done, and the topology of your cluster.
 * Invoked via the -q switch.
 */

void
do_showcluster(int fanout)
{
    node_t *nodeptr;
    int i, j, l, n;
    char *group;

    i = l = 0;
    if (nodelink == NULL)
	return;

    for (nodeptr = nodelink; nodeptr->next != NULL;
	 nodeptr = nodeptr->next)
	i++; /* just count the nodes */
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

    nodeptr = nodelink;

    if (getenv("CLUSTER"))
	(void)printf("Cluster file: %s\n", getenv("CLUSTER"));
    (void)printf("Fanout size: %d\n", fanout);
    for (n=0; n <= j; n++) {
	for (i=0; (i < fanout && nodeptr != NULL); i++) {
	    l++;
	    group = NULL;
	    if (nodeptr->group >= 0 && grouplist &&
		grouplist[nodeptr->group].name)
		group = strdup(grouplist[nodeptr->group].name);
	    if (group == NULL)
		(void)printf("Node: %3d  Fangroup: %3d  Rungroup: None"
		    "             Host: %-15s\n", l, n + 1, nodeptr->name);
	    else
		(void)printf("Node: %3d  Fangroup: %3d  Rungroup: %-15s"
		    "  Host: %-15s\n", l, n + 1, group,
		    nodeptr->name);
	    nodeptr = nodeptr->next;
	}
    }
}


/*
 * A routine to parse the command arguments, and prepare a nodelist for use
 * returns the number of groups in the list.
 */

int
parse_cluster(char **exclude)
{
    FILE *fd;
    char *clusterfile, *p, *nodename, *q;
    int i, j, g, fail, gfail, lumping, n, ging;
    char buf[MAXBUF];
    extern int errno;
    group_t *grouptemp;
    node_t *nodeptr;
    char **lumptemp;

    g = 0;
    lumping = ging = 0;
    n = 0;
    gfail = 1;

    grouplist = (group_t *)calloc(GROUP_MALLOC, sizeof(group_t));
    if (grouplist == NULL)
	bailout();

    lumplist = (char **)calloc(GROUP_MALLOC, sizeof(char *));
    if (lumplist == NULL)
	bailout();

    /* if -w wasn't specified, we need to parse the cluster file */
    clusterfile = getenv("CLUSTER");
    if (clusterfile == NULL) {
	(void)fprintf(stderr,
	    "%s: must use -w flag without CLUSTER environment setting.\n",
	    progname);
	exit(EXIT_FAILURE);
    }
    fd = fopen(clusterfile, "r");
    if (NULL == fd) {
	(void)fprintf(stderr, "%s: open of clusterfile failed:%s\n",
	    progname, strerror(errno));
	exit(EXIT_FAILURE);
    }

    /* this is horrid. rewrite me in lex/yacc */

    /* First, find all the groups, and build a grouplist */
    while ((nodename = fgets(buf, sizeof(buf), fd))) {
	p = (char *)strsep(&nodename, "\n");
	if ((strcmp(p, "") != 0) && (strncmp(p, "#", 1) != 0)) {
	    if (strstr(p, "GROUP") != NULL) {
		ging = 1;
		strsep(&p, ":");
		while (isspace((unsigned char)*p))
		    p++;
		if (((g+1) % GROUP_MALLOC) != 0) {
		    grouplist[g].name = strdup(p);
		    grouplist[g].numlump = 0;
		} else {
		    grouptemp = realloc(grouplist,
			(g+1+GROUP_MALLOC)*sizeof(group_t));
		    if (grouptemp != NULL)
			grouplist = grouptemp;
		    else
			bailout();
		    grouplist[g].name = strdup(p);
		    grouplist[g].numlump = 0;
		}
		/* kill trailing whitespace */
		q = grouplist[g].name;
		q += strlen(grouplist[g].name)-1;
		while (isspace((unsigned char)*q))
		    q--;
		q++;
		*q = '\0';
		g++;
	    } else if (ging && ((strstr(p, "GROUP") != NULL) ||
			   (strstr(p, "LUMP") != NULL)))
		ging = 0;
	}
    }
    rewind(fd);
    /* now build the lump list */
    lumplist[0] = strdup("NO_GROUP_HERE");
    if (lumplist[0] == NULL)
	bailout();
    while ((nodename = fgets(buf, sizeof(buf), fd))) {
	p = (char *)strsep(&nodename, "\n");
	/* check for a null line, or a comment */
	if ((strcmp(p, "") != 0) && (strncmp(p, "#", 1) != 0)) {
	    if (strstr(p, "LUMP") != NULL) {
		lumping = 1;
		strsep(&p, ":");
		while (isspace((unsigned char)*p))
		    p++;
		if (((n+1) % GROUP_MALLOC) != 0) {
		    lumplist[n] = strdup(p);
		} else {
		    lumptemp = realloc(lumplist,
			(n+1+GROUP_MALLOC)*sizeof(char *));
		    if (lumptemp != NULL)
			lumplist = lumptemp;
		    else
			bailout();
		    lumplist[n] = strdup(p);
		}
		/* trim trailing whitespace */
		q = lumplist[n];
		q += strlen(lumplist[n])-1;
		while (isspace((unsigned char)*q))
			q--;
		q++;
		*q = '\0';
		n++;
	    } else if (lumping){
		if ((strstr(p, "GROUP") != NULL) ||
		    (strstr(p, "LUMP") != NULL))
		    lumping = 0;
		else {
		    for (j = 0; j < g; j++)
			if (strcmp(p, grouplist[j].name) == 0) {
			    if (grouplist[j].numlump == 0)
			        grouplist[j].lump = calloc(2, sizeof(int));
			    else
				grouplist[j].lump =
				    realloc(grouplist[j].lump, sizeof(int)*grouplist[j].numlump+2);
			    grouplist[j].lump[grouplist[j].numlump] = n-1;
			    grouplist[j].numlump++;
			}
		}
	    }
	}
    }
    rewind(fd);
    /* Now.. parse the file for real, and build the nodelist */
    g = -1;
    i = 0;
    while ((nodename = fgets(buf, sizeof(buf), fd))) {
	p = (char *)strsep(&nodename, "\n");
	while (isspace((unsigned char)*p))
		p++;
	if ((strcmp(p, "") != 0) && (strncmp(p, "#", 1) != 0)) {
	    /*printf("g = %d gfail = %d p = %s\n", g, gfail, p);*/
	    if (strstr(p, "LUMP") != NULL)
		lumping = 1;
	    if (lumping && (strstr(p, "GROUP") != NULL))
		lumping = 0;
	    if (exclusion || grouping) { /* this handles the -x,g option */
		fail = 0;
		gfail = 0;
		if (exclude != NULL) {
			for (j = 0; exclude[j] != NULL; j++)
				if (strcmp(p, exclude[j]) == 0)
					fail = 1;
		}
		if (g >= 0) {
		    gfail = 1;
		    for (j = 0; (j < nrofrungroups && gfail == 1); j++) {
			int l;
			if (!strcmp(grouplist[g].name, rungroup[j]))
			    gfail = 0;
			for (l=0; l < grouplist[g].numlump; l++)
			    if (!strcmp(lumplist[grouplist[g].lump[l]],
				    rungroup[j]))
				gfail = 0;
		    }
		}
		if (g >= 0 && gfail && exclusion && rungroup[0] == 0)
		    gfail = 0;
		if (!fail) {
		    if (strstr(p, "GROUP") != NULL) {
			g++;
		    } else if (!gfail && !lumping) {
			nodeptr = nodealloc(strdup(p));
			if (g >= 0)
			    nodeptr->group = g;
		    }
		}
	    } else {
		if (strstr(p, "GROUP") != NULL) {
		    g++;
		} else if (!lumping){
		    nodeptr = nodealloc(strdup(p));
		    if (g >= 0)
			nodeptr->group = g;
		}
	    } /* exlusion */
	} /* strcmp */
    } /* while nodename */
    fclose(fd);
    return(g);
}

/* allocates a new/first node, and returns a pointer to the user */
struct node_data *
nodealloc(char *nodename)
{
    struct node_data *nodeptr, *nodex;

    if (nodelink == NULL) {
	nodelink = calloc(1, sizeof(node_t));
	nodelink->name = strdup(nodename);
	nodelink->group = 0;
	nodelink->err.fds[0] = 0;
	nodelink->err.fds[1] = 0;
	nodelink->out.fds[0] = 0;
	nodelink->out.fds[1] = 0;
	nodelink->index = 1.0;
	nodelink->free = 1;
	nodelink->childpid = 0;
	nodelink->next = NULL;
#ifdef USE_X11
	nodelink->win_id = 0;
#endif
	return(nodelink);
    }
    nodex = calloc(1, sizeof(node_t));
    if (NULL == nodex)
	bailout();
	
    for (nodeptr = nodelink; nodeptr->next != NULL;
	 nodeptr = nodeptr->next)
	;
	
    nodeptr->next = nodex;
    nodex->name = strdup(nodename);
    nodex->group = 0;
    nodex->err.fds[0] = 0;
    nodex->err.fds[1] = 0;
    nodex->out.fds[0] = 0;
    nodex->out.fds[1] = 0;
    nodex->childpid = 0;
    nodex->next = 0;
    nodex->free = 1;
    nodex->index = 1.0;
#ifdef USE_X11
    nodex->win_id = 0;
#endif
    return(nodex);
}

void
alarm_handler(int sig)
{
	if (sig == SIGALRM)
		alarmtime = 1;
	return;
}

int
test_node_connection(int rshport, int timeout, node_t *nodeptr)
{
    int sock;
    struct sockaddr_in name;
    struct hostent *hostinfo;
    struct itimerval timer;
    struct sigaction signaler;
    
    /* test if the port exists and is serviceable */
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
	perror("socket");
	exit(EXIT_FAILURE);
    }
    name.sin_family = AF_INET;
    name.sin_port = htons(rshport);
    hostinfo = gethostbyname(nodeptr->name);
    if (hostinfo == NULL) {
	(void)fprintf(stderr, "Unknown host %s.\n", nodeptr->name);
	close(sock);
	return(0);
    }
    name.sin_addr = *(struct in_addr *) hostinfo->h_addr;
    alarmtime = 0;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;
    timer.it_value.tv_sec = timeout;
    timer.it_value.tv_usec = 0;
    signaler.sa_handler = alarm_handler;
    signaler.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &signaler, NULL);
    setitimer(ITIMER_REAL, &timer, NULL);
    if (connect(sock, (struct sockaddr *)&name,	sizeof(name)) != 0) {
	if (alarmtime)
	    fprintf(stderr, "Connection timed out to port %d on %s\n",
		rshport, nodeptr->name);
	else
	    fprintf(stderr, "Cannot connect to port %d on %s\n",
		rshport, nodeptr->name);
	close(sock);
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = 0;
	setitimer(ITIMER_REAL, &timer, NULL);
	return(0);
    }
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &timer, NULL);
    close(sock);
    return(1);
}

#endif /* CLUSTERS */


/* convenience:  Set the rshport based on RCMD_CMD */
int
get_rshport(int testflag, int rshport, char *rcmd_env)
{
	int port;

	port = TEST_PORT;
	
	/* we need to find or guess the port number */
	if (testflag && rshport == 0) {
		if (!getenv(rcmd_env))
			port = TEST_PORT;
		else if (strstr(getenv(rcmd_env), "ssh") != NULL)
			port = 22;
		else if (strstr(getenv(rcmd_env), "rsh") != NULL)
			port = 514;
		else if (strstr(getenv(rcmd_env), "scp") != NULL)
			port = 22;
		else if (strstr(getenv(rcmd_env), "rcp") != NULL)
			port = 514;
		else if (strstr(getenv(rcmd_env), "rlogin") != NULL)
			port = 513;
		else {
			(void)fprintf(stderr,
			    "-t argument given, but port number to test "
			    "could not be guessed.  Please set the RCMD_PORT "
			    "environment variable to the portnumber of the "
			    "protocol you are using, or supply it with the "
			    "-p (or -n for pcp) argument.\n");
			exit(EXIT_FAILURE);
		}
		if (debug)
			printf("Test port: %d\n", port);
	}
	return(port);
}

/* Convenience:  Build the rshstring.
 * Caller must free return value.
 */
char *
build_rshstring(char **rsh, int nrofargs)
{
	int g, i;
	char *rshstring;

	/* build the rshstring (for debug printf and dsh -s) */
	for (g=0, i=0; i < nrofargs-2; i++) {
		if (rsh[i] != NULL)
			g += strlen(rsh[i]);
	}
	/* padding of 20 for pcp/etc */
	rshstring = (char *)malloc(sizeof(char) * (g + nrofargs + 20));
	sprintf(rshstring, "%s", rsh[0]);
	for (i=1; i < nrofargs-2; i++) {
		if (rsh[i] != NULL)
			sprintf(rshstring, "%s %s", rshstring, rsh[i]);
	}

	return(rshstring);
}


/*
 * Return the default RCMD_CMD, based on the env var and dsh setup
 */
char *
default_rcmd(char *rcmd_env)
{
	if (strcmp(rcmd_env, "RCMD_CMD") == 0)
		return(RCMD_DEFAULT);
	if (strcmp(rcmd_env, "RLOGIN_CMD") == 0)
		return(RLOGIN_DEFAULT);
	if (strcmp(rcmd_env, "RCP_CMD") == 0)
		return(RCP_DEFAULT);
	if (strcmp(rcmd_env, "RVT_CMD") == 0)
		return(RVT_DEFAULT);
	return("rsh");
}

/* This routine parses the RCMD_CMD and RCMD_CMD_ARGS environment variables
 * and tries to set things up properly for them.
 */

char **
parse_rcmd(char *rcmd_env, char *args_env, int *nrofargs)
{
	int i, j, a;
	char *p, *tmp, **cmd;

	tmp = getenv(args_env);
	p = tmp;
	a = 1;
	j = 3;
	
	if (tmp != NULL) {
		while (*p != '\0') {
			if (isspace((unsigned char)*p))
				a++;
			p++;
		}
	} else
		a = 0;

	tmp = getenv(rcmd_env);
	p = tmp;
	if (tmp != NULL) {
		while (*p != '\0') {
			if (isspace((unsigned char)*p))
				j++;
			p++;
		}
		cmd = malloc(sizeof(char *) * (j+1+a));
		i = 0;
		while (tmp != NULL)
			cmd[i++] = strdup(strsep(&tmp, " "));
		cmd[i] = (char *)0;
		*nrofargs = j+a;
	} else {
		cmd = malloc(sizeof(char *) * (j+1+a));
		cmd[0] = strdup(default_rcmd(rcmd_env));
		cmd[1] = (char *)0;
		i = 1;
		*nrofargs = j+a;
	}

	tmp = getenv(args_env);
	while (tmp != NULL)
		cmd[i++] = strdup(strsep(&tmp, " "));
	cmd[i] = (char *)0;

	return(cmd);
}

/* 
 * Simple error handling routine, needs severe work.
 * Its almost totally useless.
 */

/*ARGSUSED*/
void
_bailout(int line, char *file)
{
    extern int errno;
	
    (void)fprintf(stderr, "%s: Failed in %s line %d: %s %d\n",
		  progname, file, line, strerror(errno), errno);
    _exit(EXIT_FAILURE);
}
