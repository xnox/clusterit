/* $Id: dtop.c,v 1.5 2008/02/27 18:50:34 garbled Exp $ */
/*
 * Copyright (c) 1998, 1999, 2000, 2007
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
#include <curses.h>

#include "../common/common.h"

#if !defined(lint) && defined(__NetBSD__)
__COPYRIGHT(
"@(#) Copyright (c) 1998, 1999, 2000\n\
        Tim Rightnour.  All rights reserved\n");
__RCSID("$Id: dtop.c,v 1.5 2008/02/27 18:50:34 garbled Exp $");
#endif /* not lint */

#define DPRINTF if (debug) printf

typedef struct procdata {
	node_t *node;
	int pid;
	char username[9];
	int size;
	int res;
	float cpu;
	char command[30];
} procdata_t;

typedef struct nodedata {
	node_t *node;
	long long int actmem;
	long long int inactmem;
	long long int wiredmem;
	long long int execmem;
	long long int filemem;
	long long int freemem;
	long long int swap;
	long long int swapfree;
	int procs;
	float load1, load5, load15;
} nodedata_t;

void do_command(int fanout, char *username);
void sig_handler(int i);

#define TOP_COMMAND "/bin/sh -c \"if [ `uname` = \"Linux\" ]; then top -bn 1; else top -Sb 2000; fi\""
#define SORT_CPU 0
#define SORT_SIZE 1
#define SORT_RES 2
#define SORT_CPU_ONENODE 3
#define SORT_MEM_ONENODE 4
#define SORT_HOSTNAME 5
#define SORT_LOADAVG 6
#define SORT_ACTIVE 7
#define SORT_INACTIVE 8
#define SORT_FILE 9
#define SORT_FREE 10
#define SORT_SWAP 11

#define DISPLAY_PROC  1
#define DISPLAY_LOAD  2

#define TOP_NORMAL 0
#define TOP_PROCPS 1
#define TOP_NORMAL_THR 2

/* globals */
int debug, exclusion, grouping, interval;
int testflag, rshport, porttimeout, batchflag;
int showprocs;
node_t *nodelink;
group_t *grouplist;
char **rungroup;
int nrofrungroups;
char **lumplist;
pid_t currentchild;
char *progname;
volatile sig_atomic_t alarmtime, exitflag;
nodedata_t *nodedata, *sortnode;
procdata_t **procdata, **sortedprocs;
int totalpids, sortmethod=SORT_CPU;
int displaymode=DISPLAY_PROC;
int topmode=0;
size_t maxnodelen;
WINDOW *stdscr;

/*
 * Dtop is program that runs top across an entire cluster, and displays
 * the biggest hogs on each node.
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
    exclusion = debug = batchflag = 0;
    testflag = rshport = nrofrungroups = 0;
    showprocs = 20; /* nrof procs to show by default */
    interval = 5; /* 5 second delay between node reads */
    porttimeout = 5; /* 5 seconds to port timeout */
    grouping = 0;
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
    while ((ch = getopt(argc, argv, "+?bdqti:f:g:l:m:o:p:s:vw:x:")) != -1)
#else
    while ((ch = getopt(argc, argv, "?bdqti:f:g:l:m:o:p:s:vw:x:")) != -1)
#endif
	switch (ch) {
	case 'b':
	    batchflag = 1;      /* we want batch mode */
	    break;
	case 'd':		/* we want to debug dsh (hidden)*/
	    debug = 1;
	    break;
	case 'i':		/* interval between node reads */
	    interval = atoi(optarg);
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
	case 'm':               /* set the display mode */
		if (strncasecmp(optarg, "proc", 4) == 0)
			displaymode = DISPLAY_PROC;
		else if (strncasecmp(optarg, "load", 4) == 0)
			displaymode = DISPLAY_LOAD;
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
	        "usage:\n%s [-bqtv] [-f fanout] [-p portnum] [-o timeout] "
		"[-g rungroup1,...,rungroupN]\n"
		"     [-l username] [-x node1,...,nodeN] [-w node1,..,nodeN]"
		" [-m proc|load] [-i interval]\n", progname);
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

    do_command(fanout, username);
    return(EXIT_SUCCESS);
}

long long int
dehumanize_number(char *num)
{
	int i;
	long long int val;

	for (i=0; i < 30; i++) {
		switch (num[i]) {
		case 'M':
		case 'm':
			num[i] = '\0';
			val = atoll(num) * 1024 * 1024;
			return val;
		case 'K':
		case 'k':
			num[i] = '\0';
			val = atoll(num) * 1024;
			DPRINTF("DE: %s, val=%lld num=%lld\n", num, val, atoll(num));
			return val;
		default:
			break;
		}
	}
	return atoi(num);
}

/* some of these fields may or may not appear, so we do it all stupid like
 * which refers to 0 for Memory 1 for Swap.
 */

void
parse_mem(char *c, nodedata_t *nd, int which)
{
	char *p, *q, *r, *n, buf[30];

	n = q = strdup(c);
	q = strchr(q, ':');
	q++;
	p = strsep(&q, ",");
	while (p != NULL) {
		DPRINTF("got %s\n", p);
		if (strstr(p, "inactive") != NULL) {
			sscanf(p, "%s inactive", buf);
			nd->inactmem = dehumanize_number(buf);
			DPRINTF(" inactive final %lld\n", nd->inactmem);
		} else if (strstr(p, "swap free") != NULL) {
			sscanf(p, "%s swap free", buf);
			nd->swapfree = dehumanize_number(buf);
			nd->swap += nd->swapfree;
		} else if (strstr(p, "swap in use") != NULL) {
			sscanf(p, "%s swap in use", buf);
			nd->swap = dehumanize_number(buf);
		} else if (strstr(p, "active") != NULL) {
			sscanf(p, "%s active", buf);
			nd->actmem = dehumanize_number(buf);
			DPRINTF(" active final %lld\n", nd->actmem);
		} else if (strstr(p, "used") != NULL && !which) {
			sscanf(p, "%s used", buf);
			nd->wiredmem = dehumanize_number(buf);
			DPRINTF(" wired final %lld\n", nd->wiredmem);
		} else if (strstr(p, "shared") != NULL) {
			sscanf(p, "%s shared", buf);
			nd->execmem = dehumanize_number(buf);
			DPRINTF(" exec final %lld\n", nd->execmem);
		} else if (strstr(p, "free") != NULL && !which) {
			sscanf(p, "%s free", buf);
			nd->freemem = dehumanize_number(buf);
			DPRINTF(" memfree final %lld\n", nd->freemem);
		} else if (strstr(p, "cached") != NULL) {
			r = strstr(p, "free");
			sscanf(p, "%s free", buf);
			if (which) {
				nd->swapfree = dehumanize_number(buf);
				DPRINTF(" swapfree final %lld\n", nd->swapfree);
			}
			DPRINTF("FOO: %s %s\n", r, p);
			if (r != NULL) {
				while (!isdigit(*r))
					r++;
				sscanf(r, "%s cached", buf);
			} else
				sscanf(p, "%s cached", buf);
			nd->filemem = dehumanize_number(buf);
			DPRINTF(" cached final %lld\n", nd->filemem);
		} else if (strstr(p, "av") != NULL && which) {
			sscanf(p, "%s av", buf);
			nd->swap = dehumanize_number(buf);
			DPRINTF(" swaptotal final %lld\n", nd->swap);
		} else if (strstr(p, "Act") != NULL) {
			sscanf(p, "%s Act", buf);
			nd->actmem = dehumanize_number(buf);
			DPRINTF(" final %lld\n", nd->actmem);
		} else if (strstr(p, "Inact") != NULL) {
			sscanf(p, "%s Inact", buf);
			nd->inactmem = dehumanize_number(buf);
			DPRINTF(" final %lld\n", nd->inactmem);
		} else if (strstr(p, "Wired") != NULL) {
			sscanf(p, "%s Wired", buf);
			nd->wiredmem = dehumanize_number(buf);
			DPRINTF(" final %lld\n", nd->wiredmem);
		} else if (strstr(p, "Exec") != NULL) {
			sscanf(p, "%s Exec", buf);
			nd->execmem = dehumanize_number(buf);
			DPRINTF(" final %lld\n", nd->execmem);
		} else if (strstr(p, "File") != NULL) {
			sscanf(p, "%s File", buf);
			nd->filemem = dehumanize_number(buf);
			DPRINTF(" final %lld\n", nd->filemem);
		} else if (strstr(p, "Free") != NULL) {
			sscanf(p, "%s Free", buf);
			if (which) {
			       nd->swapfree = dehumanize_number(buf);
			       DPRINTF(" final %lld\n", nd->swapfree);
			} else {
				nd->freemem = dehumanize_number(buf);
				DPRINTF(" final %lld\n", nd->freemem);
			}
		} else if (strstr(p, "Total") != NULL) {
			sscanf(p, "%s Total", buf);
			nd->swap = dehumanize_number(buf);
			DPRINTF(" final %lld\n", nd->swap);
		}
		p = strsep(&q, ",");
	}
	free(n);
}

void
parse_pid(char *c, node_t *node, int nn, int pid)
{
	char *p, *q, *n, buf[80];
	size_t s;
	int i, col=0;

	procdata[nn][pid].node = node;
	n = q = strdup(c);
	s = strspn(q, " ");
	q += s;
	p = strsep(&q, " ");
	while (p != NULL) {
		DPRINTF("got data column %d %s\n", col, p);
		switch (col) {
		case 0: /* pid */
			procdata[nn][pid].pid = atoi(p);
			DPRINTF("pid = %d\n", procdata[nn][pid].pid);
			break;
		case 1:
			strncpy(procdata[nn][pid].username, p, 9);
			DPRINTF("user=%s\n", procdata[nn][pid].username);
			break;
		case 4:
			if (topmode == TOP_NORMAL_THR)
				break;
			procdata[nn][pid].size = dehumanize_number(p);
			if (topmode == TOP_PROCPS)
				procdata[nn][pid].size *= 1024;
			DPRINTF("size=%d\n", procdata[nn][pid].size);
			break;
		case 5:
			if (topmode == TOP_NORMAL_THR)
				procdata[nn][pid].size = dehumanize_number(p);
			else
				procdata[nn][pid].res = dehumanize_number(p);
			if (topmode == TOP_PROCPS)
				procdata[nn][pid].res *= 1024;
			DPRINTF("res=%d\n", procdata[nn][pid].res);
			DPRINTF("size=%d\n", procdata[nn][pid].size);
			break;
		case 6:
			if (topmode == TOP_NORMAL_THR)
				procdata[nn][pid].res = dehumanize_number(p);
			DPRINTF("res=%d\n", procdata[nn][pid].res);
			DPRINTF("size=%d\n", procdata[nn][pid].size);
			break;
		case 8:
			if (topmode == TOP_PROCPS) {
				sscanf(p, "%f%%", &procdata[nn][pid].cpu);
				DPRINTF("cpu=%f\n", procdata[nn][pid].cpu);
			}
		case 9:
			if (topmode != TOP_PROCPS) {
				sscanf(p, "%f%%", &procdata[nn][pid].cpu);
				DPRINTF("cpu=%f\n", procdata[nn][pid].cpu);
			}
			break;
		case 10:
			if (topmode != TOP_PROCPS) {
				strncpy(procdata[nn][pid].command, p, 30);
				for (i=0; i < 30; i++)
					if (procdata[nn][pid].command[i] == '\n')
						procdata[nn][pid].command[i] = '\0';
				DPRINTF("comm=%s\n", procdata[nn][pid].command);
			}
			break;
		case 12:
			if (topmode == TOP_PROCPS) {
				strncpy(procdata[nn][pid].command, p, 30);
				for (i=0; i < 30; i++)
					if (procdata[nn][pid].command[i] == '\n')
						procdata[nn][pid].command[i] = '\0';
				DPRINTF("comm=%s\n", procdata[nn][pid].command);
			}
			break;
		default:
			break;
		}
			
		if (q != NULL) {
			s = strspn(q, " ");
			q += s;
		}
		p = strsep(&q, " ");
		col++;
	}
	free(n);
}

int
parse_top(char *cd, node_t *node, int nn, int pid)
{
	char *c, *d;
	int ret;

	ret = 0;
	c = cd;
	DPRINTF("enter pzarse_top for %d %s\n", nn, node->name);
	DPRINTF("LINE %s\n", cd);
	while (c != NULL) {
		if (strstr(c, "load average:") != NULL) {
			topmode = TOP_PROCPS;
			d = strstr(c, "load average:");
			nodedata[nn].node = node;
			sscanf(d, "load average: %f, %f, %f",
			    &nodedata[nn].load1, &nodedata[nn].load5,
			    &nodedata[nn].load15);
			DPRINTF("parse_top: Xfloats %f %f %f\n",nodedata[nn].load1,nodedata[nn].load5,nodedata[nn].load15 );
		} else if (strstr(c, "load averages") != NULL) {
			nodedata[nn].node = node;
			sscanf(c, "load averages: %f, %f, %f", &nodedata[nn].load1,
			    &nodedata[nn].load5, &nodedata[nn].load15);
			DPRINTF("parse_top: floats %f %f %f\n",nodedata[nn].load1,nodedata[nn].load5,nodedata[nn].load15 );
		} else if (strstr(c, "processes:") != NULL) {
			sscanf(c, "%d processes:", &nodedata[nn].procs);
			DPRINTF("parse_top: procs %d\n", nodedata[nn].procs);
			if (nodedata[nn].procs > 0) {
			    if (procdata[nn] != NULL)
				    free(procdata[nn]);
			    procdata[nn] = calloc(nodedata[nn].procs,
				sizeof(procdata_t));
			    DPRINTF("parse_top: allocated prodata array for %s\n", node->name);
			}
		} else if (strstr(c, "Memory:") != NULL) {
			parse_mem(c, &nodedata[nn], 0);
		} else if (strstr(c, "inactive") != NULL) {
			c[0] = ':';
			parse_mem(c, &nodedata[nn], 0);
		} else if (strstr(c, "Mem:") != NULL) {
			parse_mem(c, &nodedata[nn], 0);
		} else if (strstr(c, "Swap:") != NULL) {
			parse_mem(c, &nodedata[nn], 1);
		} else if (strstr(c, "  PID") != NULL) {
			if (strstr(c, " THR ") != NULL)
				topmode = TOP_NORMAL_THR;
			/* ignore this line */
		} else if (c[0] == '\n') {
			; /* ignore this line */
		} else if (isdigit(c[4])) {
			parse_pid(c, node, nn, pid);
			DPRINTF("PIDLINE\n");
			ret = 1;
		}


		
		DPRINTF("data = %s\n", c);
		c = strchr(c, '\n');
		c++;
		c = strstr(c, "\n");
	}
	return ret;
}

int
compare_node(const void *a, const void *b)
{
	const nodedata_t *one = a;
	const nodedata_t *two = b;
	float fone, ftwo;

	switch (sortmethod) {
	case SORT_HOSTNAME:
		return(strcmp(one->node->name, two->node->name));
		break;
	case SORT_LOADAVG:
		if (one->load1 > two->load1)
			return -1;
		if (one->load1 < two->load1)
			return 1;
		return 0;
		break;
	case SORT_ACTIVE:
		if (one->actmem > two->actmem)
			return -1;
		if (one->actmem < two->actmem)
			return 1;
		return 0;
		break;
	case SORT_INACTIVE:
		if (one->inactmem > two->inactmem)
			return -1;
		if (one->inactmem < two->inactmem)
			return 1;
		return 0;
		break;
	case SORT_FILE:
		if (one->filemem > two->filemem)
			return -1;
		if (one->filemem < two->filemem)
			return 1;
		return 0;
		break;
	case SORT_FREE:
		if (one->freemem > two->freemem)
			return 1;
		if (one->freemem < two->freemem)
			return -1;
		return 0;
		break;
	case SORT_SWAP:
		fone = 1.0 - ((float)one->swapfree / (float)one->swap);
		ftwo = 1.0 - ((float)two->swapfree / (float)two->swap);
		if (fone > ftwo)
			return -1;
		if (fone < ftwo)
			return 1;
		return 0;
		break;
	default:
		return 0;
	}
}

int
compare_proc(const void *a, const void *b)
{
	procdata_t * const *one = a;
	procdata_t * const *two = b;

	switch (sortmethod) {
	case SORT_SIZE:
		if ((*one)->size > (*two)->size)
			return -1;
		if ((*one)->size < (*two)->size)
			return 1;
		return 0;
		break;
	case SORT_RES:
		if ((*one)->res > (*two)->res)
			return -1;
		if ((*one)->res < (*two)->res)
			return 1;
		return 0;
		break;
	case SORT_CPU:
	default:
		if ((*one)->cpu > (*two)->cpu)
			return -1;
		if ((*one)->cpu < (*two)->cpu)
			return 1;
		return 0;
		break;
	}
}

/*
 * Generate a PID line.  idx is an index into the sortedprocs array.
 */

void
print_pid(char *obuf, int idx)
{
	int i;
	char buf[8], xbuf[80];
	const char *die = "";
	
	if (sortedprocs[idx]->node == NULL)
		return;
	sprintf(obuf, "%*s", maxnodelen, sortedprocs[idx]->node->name);
	sprintf(xbuf, " %6d", sortedprocs[idx]->pid);
	strcat(obuf, xbuf);
	sprintf(xbuf, " %8s", sortedprocs[idx]->username);
	strcat(obuf, xbuf);
	i = humanize_number(buf, 6, sortedprocs[idx]->size, die, HN_AUTOSCALE,
	    HN_NOSPACE|HN_DECIMAL);
	if (i == -1)
		sprintf(xbuf, " UNK");
	else
		sprintf(xbuf, " %6s", buf);
	strcat(obuf, xbuf);
	i = humanize_number(buf, 6, sortedprocs[idx]->res, "", HN_AUTOSCALE,
	    HN_NOSPACE);
	if (i == -1)
		sprintf(xbuf, " UNK");
	else
		sprintf(xbuf, " %6s", buf);
	strcat(obuf, xbuf);
	sprintf(xbuf, " %5.2f%%", sortedprocs[idx]->cpu);
	strcat(obuf, xbuf);
	sprintf(xbuf, " %-30s", sortedprocs[idx]->command);
	strcat(obuf, xbuf);
}

/*
 * Generate a nodedata line, idx is nodenum
 */

void
print_nodedata(char *obuf, int idx)
{
	int i;
	float f;
	char buf[8], xbuf[80];

	sprintf(obuf, "%*s %6d %6.2f %6.2f %6.2f", maxnodelen,
	    sortnode[idx].node->name, sortnode[idx].procs,
	    sortnode[idx].load1, sortnode[idx].load5,
	    sortnode[idx].load15);
	i = humanize_number(buf, 6, sortnode[idx].actmem, "", HN_AUTOSCALE,
	    HN_NOSPACE);
	if (i == -1)
		sprintf(xbuf, " UNK");
	else
		sprintf(xbuf, " %6s", buf);
	strcat(obuf, xbuf);

	i = humanize_number(buf, 6, sortnode[idx].inactmem, "", HN_AUTOSCALE,
	    HN_NOSPACE);
	if (i == -1)
		sprintf(xbuf, " UNK");
	else
		sprintf(xbuf, " %6s", buf);
	strcat(obuf, xbuf);

	i = humanize_number(buf, 6, sortnode[idx].filemem, "", HN_AUTOSCALE,
	    HN_NOSPACE);
	if (i == -1)
		sprintf(xbuf, " UNK");
	else
		sprintf(xbuf, " %6s", buf);
	strcat(obuf, xbuf);

	i = humanize_number(buf, 6, sortnode[idx].freemem, "", HN_AUTOSCALE,
	    HN_NOSPACE);
	if (i == -1)
		sprintf(xbuf, " UNK");
	else
		sprintf(xbuf, " %6s", buf);
	strcat(obuf, xbuf);

	i = humanize_number(buf, 6, sortnode[idx].swapfree, "", HN_AUTOSCALE,
	    HN_NOSPACE);
	if (i == -1)
		sprintf(xbuf, " UNK");
	else
		sprintf(xbuf, " %6s", buf);
	strcat(obuf, xbuf);

	f = 1.0 - ((float)sortnode[idx].swapfree / (float)sortnode[idx].swap);
	f *= 100.0;
	sprintf(xbuf, " %5.2f%%", f);
	strcat(obuf, xbuf);
}

/*
 * Simple ascii print of top, for use in batch mode, etc
 */

void
batch_print(int tprocs, int tnodes)
{
	int i, vnodes;
	char buf[80];

	switch (displaymode) {
	case DISPLAY_LOAD:
		memcpy(sortnode, nodedata, sizeof(nodedata_t) * tnodes);
		for (i=0, vnodes=0; i < tnodes; i++)
			if (nodedata[i].node != NULL)
				vnodes++;
		qsort(sortnode, vnodes, sizeof(nodedata_t), compare_node);
		printf("%*s %6s %6s %6s %6s %6s %6s %6s %6s %6s %6s\n",
		    maxnodelen, "HOSTNAME", "PROCS", "LOAD1", "LOAD5",
		    "LOAD15", "ACTIVE", "INACT", "FILE", "FREE",
		    "SWPFRE", "SWUSED");
		for (i=0; i < vnodes; i++) {
			print_nodedata(buf, i);
			printf("%s\n", buf);
		}
		break;
	case DISPLAY_PROC:
	default:		
		printf("%*s %6s %8s %6s %6s %6s %s\n", maxnodelen, "HOSTNAME",
		    "PID", "USERNAME", "SIZE", "RES", "CPU", "COMMAND");
		printf("tprocs==%d showprocs==%d\n", tprocs, showprocs);
		for (i=0; i < tprocs && i < showprocs; i++) {
			print_pid(buf, i);
			printf("%s\n", buf);
		}
		break;
	}
}

void
curses_header(void)
{
	char buf[80];

	clear();
	move(1, 0);
	switch (displaymode) {
	case DISPLAY_LOAD:
		sprintf(buf, "%*s %6s %6s %6s %6s %6s %6s %6s %6s %6s %6s",
		    maxnodelen, "HOSTNAME", "PROCS", "LOAD1", "LOAD5",
		    "LOAD15", "ACTIVE", "INACT", "FILE", "FREE",
		    "SWPFRE", "SWUSED");
		break;
	case DISPLAY_PROC:
	default:
		sprintf(buf, "%*s %6s %8s %6s %6s %6s %s", maxnodelen,
		    "HOSTNAME", "PID", "USERNAME", "SIZE", "RES", "CPU",
		    "COMMAND");
		break;
	}
	addstr(buf);
	refresh();
}

void
curses_help(void)
{
	int i;

	clear();
	move(1, 0);
	printw("  p : Switch to proc view, sorted by CPU\n");
	printw("  m : Switch to proc view, sorted by MEM\n");
	printw("  l : Switch to load view, sorted by HOSTNAME\n");
	printw("  v : Switch to load view, sorted by loadavg\n");
	printw("  a : Switch to load view, sorted by active\n");
	printw("  i : Switch to load view, sorted by inact\n");
	printw("  r : Switch to load view, sorted by file\n");
	printw("  f : Switch to load view, sorted by free\n");
	printw("  s : Switch to load view, sorted by swapused\n");
	printw("  ? : Display this help\n");
	printw("  q : quit\n");
	printw("\n -- Hit any key --\n");
	timeout(-1);
	(void)getch();
	curses_header();
}

void
curses_setup(void)
{
	char buf[80];

	if (debug)
		return;
	stdscr = initscr();
	if (stdscr == NULL) {
		fprintf(stderr, "Could not initialize curses\n");
		bailout();
	}
	timeout(1000*interval);
	curses_header();
}

void
curses_print(int tprocs, int tnodes)
{
	int i, vnodes;
	char buf[80];

	if (debug)
		return;
	move(2, 0);
	clrtobot();
	refresh();
	switch (displaymode) {
	case DISPLAY_LOAD:
		memcpy(sortnode, nodedata, sizeof(nodedata_t) * tnodes);
		for (i=0, vnodes=0; i < tnodes; i++)
			if (nodedata[i].node != NULL)
				vnodes++;
		qsort(sortnode, vnodes, sizeof(nodedata_t), compare_node);
		for (i=0; i < vnodes && i < (LINES-2); i++) {
			print_nodedata(buf, i);
			move(i+2, 0);
			addstr(buf);
		}
		break;
	case DISPLAY_PROC:
	default:
		for (i=0; i < tprocs && i < (LINES-2); i++) {
			print_pid(buf, i);
			move(i+2, 0);
			addstr(buf);
		}
		break;
	}
	move(0, 0);
	refresh();
}

void
curses_getkey(void)
{
	int key;

	timeout(1000 * interval);
	key = getch();
	move(0,0); printw("key=%d", key);
	switch (key) {
	case 'p':
		displaymode = DISPLAY_PROC;
		sortmethod = SORT_CPU;
		break;
	case 'm':
		displaymode = DISPLAY_PROC;
		sortmethod = SORT_SIZE;
		break;
	case 'l':
		displaymode = DISPLAY_LOAD;
		sortmethod = SORT_HOSTNAME;
		break;
	case 'h':
		displaymode = DISPLAY_LOAD;
		sortmethod = SORT_HOSTNAME;
		break;
	case 'v':
		displaymode = DISPLAY_LOAD;
		sortmethod = SORT_LOADAVG;
		break;
	case 'a':
		displaymode = DISPLAY_LOAD;
		sortmethod = SORT_ACTIVE;
		break;
	case 'i':
		displaymode = DISPLAY_LOAD;
		sortmethod = SORT_INACTIVE;
		break;
	case 'r':
		displaymode = DISPLAY_LOAD;
		sortmethod = SORT_FILE;
		break;
	case 'f':
		displaymode = DISPLAY_LOAD;
		sortmethod = SORT_FREE;
		break;
	case 's':
		displaymode = DISPLAY_LOAD;
		sortmethod = SORT_SWAP;
		break;
	case 'q':
		exitflag = 1;
		break;
	case ERR:
		break;
	case '?':
	default:
		curses_help();
		break;
	}
	if (key != ERR)
		curses_header();
}


/* 
 * Do the actual dirty work of the program, now that the arguments
 * have all been parsed out.
 */

void
do_command(int fanout, char *username)
{
    struct sigaction signaler;
    FILE *fd, *fda, *in;
    char buf[MAXBUF], cbuf[MAXBUF], pipebuf[2048];
    char *command = TOP_COMMAND;
    int status, i, j, n, g, pollret, nrofargs, arg, slen, fdf;
    int tnodes, k, l;
    char *p, **q, **rsh, *cd, **cmd, *rshstring;
    char *scriptbase, *scriptdir;
    node_t *nodeptr, *nodehold;
    struct pollfd fds[2];
    
    maxnodelen = 0;
    j = i = 0;
    in = NULL;
    cd = pipebuf;
    exitflag = 0;
    sortedprocs = NULL;

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
    if (maxnodelen < 8)
	    maxnodelen = 8;
    
    i = j; /* side effect of above */
    tnodes = i;
    j = i / fanout;
    if (i % fanout)
	j++; /* compute the # of rungroups */

    nodedata = calloc(tnodes, sizeof(nodedata_t));
    sortnode = calloc(tnodes, sizeof(nodedata_t));
    if (nodedata == NULL || sortnode == NULL)
	    bailout();

    /* the per-node array of procs */
    procdata = calloc(tnodes, sizeof(procdata_t *));
    if (procdata == NULL)
	    bailout();
    
    if (debug) {
	(void)printf("\nDo Command: %s\n", command);
	(void)printf("Fanout: %d Groups:%d\n", fanout, j);
    }

#if 0
    /* XXX this is going to be a problem */
    close(STDIN_FILENO); /* DAMN this bug took awhile to find */
    if (open("/dev/null", O_RDONLY, NULL) != 0)
	    bailout();
#endif
    signaler.sa_handler = sig_handler;
    signaler.sa_flags = 0;
    sigaction(SIGTERM, &signaler, NULL);
    sigaction(SIGINT, &signaler, NULL);
    
    rsh = parse_rcmd("RCMD_CMD", "RCMD_CMD_ARGS", &nrofargs);
    rshstring = build_rshstring(rsh, nrofargs);

    if (!batchflag)
	    curses_setup();
    
    /* begin the processing loop */
    while (!exitflag) {
	g = 0;
        nodeptr = nodelink;
        for (n=0; n <= j; n++) { /* fanout group */
	    if (exitflag)
		    goto out;
	    nodehold = nodeptr;
	    for (i=0; (i < fanout && nodeptr != NULL); i++) {
		g++;
		if (exitflag)
			goto out;
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
			    _exit(EXIT_SUCCESS);
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
	        int pid=0;
		    
		if (exitflag)
			goto out;
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

//		DPRINTF("HELLO: n=%d %d %d - %d %d\n", n, g-fanout+i, i, g, fanout);
		DPRINTF("HELLO: %d\n", n*fanout + i);
		while (pollret >= 0) {
		    int gotdata;

		    pollret = poll(fds, 2, 5);
		    gotdata = 0;
		    if ((fds[0].revents&POLLIN) == POLLIN ||
			(fds[0].revents&POLLHUP) == POLLHUP ||
		        (fds[0].revents&POLLPRI) == POLLPRI) {
#ifdef __linux__
			cd = fgets(pipebuf, sizeof(pipebuf), fda);
			if (cd != NULL) {
#else
			while ((cd = fgets(pipebuf, sizeof(pipebuf), fda))) {
#endif
				pid += parse_top(cd, nodeptr,
				    n*fanout + i, pid);
			    gotdata++;
			}
		    }
		    if ((fds[1].revents&POLLIN) == POLLIN ||
			(fds[1].revents&POLLHUP) == POLLHUP ||
			(fds[1].revents&POLLPRI) == POLLPRI) {
#ifdef __linux__
			cd = fgets(pipebuf, sizeof(pipebuf), fd);
			if (cd != NULL) {
#else
			while ((cd = fgets(pipebuf, sizeof(pipebuf), fd))) {
#endif
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
	    totalpids = 0;
	    for (k=0; k < tnodes; k++) {
		    totalpids += nodedata[k].procs;
		    DPRINTF("k=%d procs=%d\n", k, nodedata[k].procs);
	    }
	    DPRINTF(" total of %d pids\n", totalpids);
	    if (sortedprocs != NULL)
		    free(sortedprocs);
	    sortedprocs = calloc(totalpids, sizeof(procdata_t *));
	    i = 0;
	    DPRINTF("tnodes=%d\n", tnodes);
	    for (k=0; k < tnodes; k++) {
		    DPRINTF("node %d procs==%d\n", k, nodedata[k].procs);
		    for (l=0; l < nodedata[k].procs; l++) {
			    if (procdata[k][l].node != NULL &&
				strcmp(procdata[k][l].command, "")) {
				    sortedprocs[i] = &procdata[k][l];
				    DPRINTF("pdata=%p\n", &procdata[k][l]);
				    i++;
			    }
		    }
	    }
	    qsort(sortedprocs, i, sizeof(procdata_t *), compare_proc);
#if 0
	    for (k=0; k < i; k++)
		    DPRINTF("node=%s, pid=%d, cpu=%f\n",
			sortedprocs[k]->node->name, sortedprocs[k]->pid,
			sortedprocs[k]->cpu);
#endif
	    if (!batchflag && !debug) {
		    curses_print(i, tnodes);
		    curses_getkey();
	    }
	} /* for n */
	DPRINTF("fanout done\n");
	if (batchflag == 1 || debug) {
		batch_print(i, tnodes);
		exitflag = 1;
	}
    } /* while loop */
out:
    if (!batchflag && !debug) {
	    move(LINES, 0);
	    endwin();
    }
    
    free(rshstring);
    for (i=0; rsh[i] != NULL; i++)
	    free(rsh[i]);
    free(rsh);
}

void
sig_handler(int i)
{
	switch (i) {
	case SIGINT:
	case SIGTERM:
		exitflag = 1;
		break;
	default:
		bailout();
		break;
	}
}
