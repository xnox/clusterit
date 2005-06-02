/* $Id: common.c,v 1.15 2005/06/02 17:01:52 garbled Exp $ */
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
__RCSID("$Id: common.c,v 1.15 2005/06/02 17:01:52 garbled Exp $");
#endif


#ifdef CLUSTERS
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
    char *clusterfile, *p, *nodename;
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

    grouplist = (group_t *)malloc(GROUP_MALLOC * sizeof(group_t));
    if (grouplist == NULL)
	bailout();

    lumplist = (char **)malloc(GROUP_MALLOC * sizeof(char *));
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
		n++;
	    } else if (lumping){
		if ((strstr(p, "GROUP") != NULL) ||
		    (strstr(p, "LUMP") != NULL))
		    lumping = 0;
		else {
		    for (j = 0; j < g; j++)
			if (strcmp(p, grouplist[j].name) == 0) {
			    if (grouplist[j].numlump == 0)
			        grouplist[j].lump = malloc(sizeof(int)*2);
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
	if ((strcmp(p, "") != 0) && (strncmp(p, "#", 1) != 0)) {
	    /* printf("g = %d p = %s\n", g, p); */
	    if (strstr(p, "LUMP") != NULL)
		lumping = 1;
	    if (lumping && (strstr(p, "GROUP") != NULL))
		lumping = 0;
	    if (exclusion || grouping) { /* this handles the -x,g option */
		fail = 0;
		for (j = 0; exclude[j] != NULL; j++)
		    if (strcmp(p, exclude[j]) == 0)
			fail = 1;
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
	nodelink = malloc((size_t)sizeof(node_t));
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
    nodex = malloc(sizeof(node_t));
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

int test_node_connection(int rshport, int timeout, node_t *nodeptr)
{
    int sock;
    struct sockaddr_in name;
    struct hostent *hostinfo;
    struct itimerval timer;
    
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
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;
    timer.it_value.tv_sec = timeout;
    timer.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &timer, NULL);
    if (connect(sock, (struct sockaddr *)&name,
				sizeof(name)) != 0) {
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
