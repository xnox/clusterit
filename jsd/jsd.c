/* $Id: jsd.c,v 1.3 2000/01/17 04:01:25 garbled Exp $ */
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

/* Semi-intelligent job scheduling daemon.  Intended for heterogenous
 * network load sharing applications.
 *
 *
 * Note for the casual observer.  While this program does indeed share a
 * large number of routines with dsh, and common.c, it is *vital* that it
 * remain totally separate.  There are some minor differences in the routines
 * and they are not compatible.
 */

#include <syslog.h>
#include <signal.h>
#include <setjmp.h>

#include "../dsh/common.h"
#include "jcommon.h"

#if !defined(lint) && defined(__NetBSD__)
__COPYRIGHT(
"@(#) Copyright (c) 1998, 1999, 2000\n\
        Tim Rightnour.  All rights reserved\n");
__RCSID("$Id: jsd.c,v 1.3 2000/01/17 04:01:25 garbled Exp $");
#endif

extern int errno;
#ifndef __P
#define __P(protos) protos
#endif

/* globals */
int debug, errorflag, exclusion, sharedmem, grouping, portnum;
int gotsigterm, gotsigusr1, gotsigusr2;
jmp_buf jmpenv;
char **grouplist;
node_t *nodelink;
char **rungroup;
pid_t currentchild;
char *progname;

void log_bailout __P((int));
void do_bench_command __P((char *, int, char *));
void sig_handler __P((int));
void main_loop __P((void));
void free_node __P((int, int));

/*
 * Main should handle deciding which group or nodes we run on, running the
 * initial benchmark run, and finally launching the scheduler daemon.
 */

int
main(argc, argv) 
	int argc;
	char *argv[];
{
	extern char	*optarg;
	extern int	optind;

	int someflag, ch, i, fanout, showflag, fanflag;
	char *p, *q, *group, *nodename, *username, *benchcmd;
	char **grouptemp, **exclude;
	struct rlimit	limit;
	pid_t pid;

	someflag = showflag = fanflag = 0;
	gotsigusr1 = gotsigterm = gotsigusr2 = 0;
	exclusion = debug = 0;
	sharedmem = 1;
	fanout = DEFAULT_FANOUT;
	nodename = NULL;
	username = NULL;
	group = NULL;
	nodelink = NULL;

	rungroup = malloc(sizeof(char **) * GROUP_MALLOC);
	if (rungroup == NULL)
		bailout(__LINE__);
	exclude = malloc(sizeof(char **) * GROUP_MALLOC);
	if (exclude == NULL)
		bailout(__LINE__);

	progname = p = q = argv[0];
	while (progname != NULL) {
		q = progname;
		progname = (char *)strsep(&p, "/");
	}
	progname = strdup(q);

	while ((ch = getopt(argc, argv, "?diqf:g:l:w:x:p:")) != -1)
		switch (ch) {
		case 'd':		/* we want to debug jsd (hidden)*/
			debug = 1;
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
		case 'f':		/* set the fanout size */
			fanout = atoi(optarg);
			fanflag = 1;
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
							bailout(__LINE__);
						rungroup[i++] = strdup(group);
					}
				}
			}
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
							bailout(__LINE__);
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
		case 'p':		/* port to listen on, shuts off SHM */
			sharedmem = 0;
			portnum = atoi(optarg);
			break;
		case '?':		/* you blew it */
			(void)fprintf(stderr,
			    "usage: jsd [-iq] [-p port] [-f fanout] "
				"[-g rungroup1,...,rungroupN] "
			    "[-l username] [-x node1,...,nodeN] [-w node1,..,nodeN] "
			    "[command ...]\n");
			return(EXIT_FAILURE);
			/*NOTREACHED*/
			break;
		default:
			break;
	}
	if (!fanflag)
/* check for a fanout var, and use it if the fanout isn't on the commandline */
		if (getenv("FANOUT"))
			fanout = atoi(getenv("FANOUT"));

	if (!someflag)
		parse_cluster(exclude);

	argc -= optind;
	argv += optind;
	if (showflag) {
		do_showcluster(fanout);
		return(EXIT_SUCCESS);
	}
	openlog("jsd", LOG_PID|LOG_NDELAY, LOG_DAEMON);

	/*
	 * set per-process limits for max descriptors, this avoids running
	 * out of descriptors and the odd errors that come with that.
	 */
	close(STDIN_FILENO);
	pid = fork();
	if (pid == 0) {
		(void)setsid(); /* disassociate from our controlling terminal */
		syslog(LOG_INFO, "Job Scheduling Daemon Started");

		if (getrlimit(RLIMIT_NOFILE, &limit) != 0)
			bailout(__LINE__);
		if (limit.rlim_cur < fanout * 5) {
			limit.rlim_cur = fanout * 5;
			if (setrlimit(RLIMIT_NOFILE, &limit) != 0)
				bailout(__LINE__);
		}
		
		if (getenv("JSD_BENCH_CMD")) {
			benchcmd = getenv("JSD_BENCH_CMD");
				do_bench_command(benchcmd, fanout, username);
		} else {
			syslog(LOG_WARNING, "No JSD_BENCH_CMD environment setting,"
				" assuming homogenus cluster.");
		}

		/* jump to the loop */
		main_loop();

	} else if (pid > 0) {
		(void)printf("%d\n", pid); /* spit the pid out */
		return(EXIT_SUCCESS);
	}
	else if (pid == -1)
		syslog(LOG_CRIT, "Aborting: %m");
	return(EXIT_FAILURE);
}

/*
 * This performs the bulk of the program's purpose.  The program enters this
 * loop, and either watches the shared memory, and talks with jsh processes,
 * or listens on the jsd port, and listens for jsh processes asking for a
 * node handout.
 *
 * The theory of operation is simple:
 * a jsh process is invoked to run a command, similar to run.  jsh contacts
 * the jsd asking for an available node.  jsd locks the database, assigns a
 * node, marks that node as in use, and unlocks the db.  Eventually the
 * jsh process will complete it's task, and report back to jsd.  Jsd will
 * then free up the node for further processing.
 */

void
main_loop()
{
	int semid, shmid, shpid;
	char *buf;
	pid_t *pid;
	node_t *nodeptr, *fastnode;
	double topspeed;
	struct sigaction signaler;
	int semdat[4] = { 1, 1, 1, 1 };

	extern int gotsigterm, gotsigusr1, gotsigusr2;
	extern jmp_buf jmpenv;

	semid = shmid = shpid = 0;
	buf = NULL;

	signaler.sa_handler = sig_handler;
	signaler.sa_flags |= SA_RESTART;
	if (sigaction(SIGTERM, &signaler, NULL) != 0)
		log_bailout(__LINE__);
	if (sigaction(SIGUSR1, &signaler, NULL) != 0)
		log_bailout(__LINE__);
	if (sigaction(SIGUSR2, &signaler, NULL) != 0)
		log_bailout(__LINE__);
	
	if (sharedmem) {
		semid = semget((key_t)getpid(), 4,
			IPC_CREAT|S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
		if (semid == -1)
			log_bailout(__LINE__);

		shmid = shmget((key_t)getpid(), MAXBUF * sizeof(char), 
			IPC_CREAT|S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
		if (shmid == -1)
			log_bailout(__LINE__);
		
		shpid = shmget((key_t)getpid(), sizeof(pid_t), 
			IPC_CREAT|S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
		if (shpid == -1)
			log_bailout(__LINE__);
		
/* 		if (semctl(semid, 0, SETALL, semdat) == -1) */
/* 			log_bailout(__LINE__); */

		if (semctl(semid, 0, SETVAL, 1) == -1)
			log_bailout(__LINE__);
		if (semctl(semid, 1, SETVAL, 1) == -1)
			log_bailout(__LINE__);
		if (semctl(semid, 2, SETVAL, 1) == -1)
			log_bailout(__LINE__);
		if (semctl(semid, 3, SETVAL, 1) == -1)
			log_bailout(__LINE__);

		buf = shmat(shmid, NULL, 0);
		if ((int)buf == -1)
			log_bailout(__LINE__);
		memcpy(buf, "\0", MAXBUF * sizeof(char)); /* init the buffer */
		pid = shmat(shpid, NULL, 0);
		*pid = getpid();

	} else {
		syslog(LOG_ERR, "Ports not implemented yet");
		_exit(EXIT_FAILURE);
	}

	setpriority(PRIO_PROCESS, 0, 20); /* nice ourselves */
	for(;;) {
		if (gotsigusr2 && !setjmp(jmpenv) && sharedmem)
			free_node(semid, shmid);
		else if (gotsigusr2)
			gotsigusr2--;
		if (gotsigterm) {
			syslog(LOG_INFO, "Got SIGTERM, cleaning up.");
			if (sharedmem) {
				semctl(semid, 0, IPC_RMID, NULL);
				shmctl(shmid, IPC_RMID, NULL);
				shmctl(shpid, IPC_RMID, NULL);
			}
			_exit(EXIT_SUCCESS);
			/* jsh will send a sigusr, to tell jsd it is about to request
			   something */
		} else if (gotsigusr1 && sharedmem) {
			if (debug)
				syslog(LOG_DEBUG, "Got SIGUSR1 in sharedmemory mode");
			shm_take_deeplock(semid);
			shm_wait_mainlock(semid);
			if (debug)
				syslog(LOG_DEBUG, "Entering deep lock");
			if (debug)
				syslog(LOG_DEBUG, "Jsh took main lock");
			topspeed = 0.0;
			fastnode = nodeptr = NULL;
			while (fastnode == NULL)
				for (nodeptr = nodelink; nodeptr; nodeptr = nodeptr->next) {
					printf("nodeptr->name = %s top=%f index=%f free=%d\n",
						nodeptr->name, topspeed, nodeptr->index,
						nodeptr->free);
					if (gotsigusr2 && !setjmp(jmpenv))
						free_node(semid, shmid);
					else if (gotsigusr2)
						gotsigusr2--;

					if (nodeptr->index > topspeed && nodeptr->free)
						fastnode = nodeptr;
				}
			(void)sprintf(buf, "%s", fastnode->name);
			if (debug)
				syslog(LOG_DEBUG, "found node %s", fastnode->name);
			fastnode->free = 0; /* mark the node as in use */
			gotsigusr1--; /* mark the event handled */
			shm_leave_deeplock(semid);

			if (debug)
				syslog(LOG_DEBUG, "leaving deep lock");
			/* jsh takes care of the other lock for us */
		}
	}
}

void
free_node(semid, shmid)
	int semid, shmid;
{
	node_t *nodeptr;
	char *buf;

	extern jmp_buf jmpenv;

	buf = shmat(shmid, NULL, 0);
	if ((int)buf == -1)
		log_bailout(__LINE__);

	if (debug)
		syslog(LOG_DEBUG, "entering free_node");
	shm_wait_freelock(semid);
	shm_take_seclock(semid);
	shm_take_freelock(semid);
	if (debug)
		syslog(LOG_DEBUG, "took seclock");
	if (debug)
		syslog(LOG_DEBUG, "found freelock: %s", buf);
	for (nodeptr = nodelink; nodeptr; nodeptr = nodeptr->next)
		if (strcmp(buf, nodeptr->name) == 0) {
			nodeptr->free = 1;
			if (debug)
				syslog(LOG_DEBUG, "freeing node %s", nodeptr->name);
		}
	memcpy(buf, "\0", MAXBUF * sizeof(char)); /* reinit the buffer */
	if (shmdt(buf) != 0)
		log_bailout(__LINE__);

	shm_leave_seclock(semid);
	shm_leave_freelock(semid);
	if (debug)
		syslog(LOG_DEBUG, "long jumping");

	longjmp(jmpenv, 0);
}



/*
 * Note that while the below is nearly identical, it has but one purpose in
 * life, and that is to populate the index of machine speeds.
 */

void
do_bench_command(argv, fanout, username)
	char *argv;
	char *username;
	int fanout;
{
	extern int debug;

	FILE *fd, *in;
	char pipebuf[2048];
	int status, i, j, n, g;
	char *q, *rsh, *cd;
	node_t *nodeptr, *nodehold;

	j = i = 0;
	in = NULL;
	q = NULL;
	cd = pipebuf;

	if (debug) {
		if (username != NULL)
			syslog(LOG_DEBUG, "As User: %s", username);
		syslog(LOG_DEBUG, "On nodes:");
	}
	for (nodeptr = nodelink; nodeptr; nodeptr = nodeptr->next) {
		if (debug) {
			q = (char *)malloc(MAXBUF * sizeof(char));
			if (q == NULL)
				log_bailout(__LINE__);
			memcpy(q, "\0", MAXBUF * sizeof(char));
			if (!(j % 4) && j > 0)
				strcat(q, "\n");
			strcat(q, nodeptr->name);
			strcat(q, "\t");
		}
		j++;
	}
	if (debug)
		syslog(LOG_DEBUG, q);

	i = j; /* side effect of above */
	j = i / fanout;
	if (i % fanout)
		j++; /* compute the # of rungroups */

	if (debug) {
		syslog(LOG_DEBUG, "\nDo Command: %s", argv);
		syslog(LOG_DEBUG, "Fanout: %d Groups:%d", fanout, j);
	}

	g = 0;
	nodeptr = nodelink;
	for (n=0; n <= j; n++) {
		nodehold = nodeptr;
		for (i=0; (i < fanout && nodeptr != NULL); i++) {
			g++;
			if (debug)
				syslog(LOG_DEBUG, "Working node: %d, fangroup %d,"
					" fanout part: %d", g, n, i);
/*
 * we set up pipes for each node, to prepare for the oncoming barrage of data.
 * Close on exec must be set here, otherwise children spawned after other
 * children, inherit the open file descriptors, and cause the pipes to remain
 * open forever.
 */
			if (pipe(nodeptr->out.fds) != 0)
				log_bailout(__LINE__);
			if (fcntl(nodeptr->out.fds[0], F_SETFD, 1) == -1)
				log_bailout(__LINE__);
			if (fcntl(nodeptr->out.fds[1], F_SETFD, 1) == -1)
				log_bailout(__LINE__);
			nodeptr->childpid = fork();
			switch (nodeptr->childpid) {
				/* its the ol fork and switch routine eh? */
			case -1:
				log_bailout(__LINE__);
				break;
			case 0:
				/* remove from parent group to avoid kernel
				 * passing signals to children.
				 */
				(void)setsid();
				if (dup2(nodeptr->out.fds[1], STDOUT_FILENO) 
					!= STDOUT_FILENO) 
					log_bailout(__LINE__);
				if (close(nodeptr->out.fds[0]) != 0)
					log_bailout(__LINE__);
				rsh = getenv("RCMD_CMD");
				if (rsh == NULL)
					rsh = "rsh";
				if (debug)
					syslog(LOG_DEBUG, "%s %s %s", rsh, nodeptr->name,
						argv);

				if (username != NULL)
/* interestingly enough, this -l thing works great with ssh */
					execlp(rsh, rsh, "-l", username, nodeptr->name,
						argv, (char *)0);
				else
					execlp(rsh, rsh, nodeptr->name, argv, (char *)0);
				log_bailout(__LINE__);
				break;
			default:
				break;
			} /* switch */
			nodeptr = nodeptr->next;
		} /* for i */
		nodeptr = nodehold;
		for (i=0; (i < fanout && nodeptr != NULL); i++) {
			if (debug)
				syslog(LOG_DEBUG, "Printing node: %d, fangroup %d,"
					" fanout part: %d", g-fanout+i, n, i);
			currentchild = nodeptr->childpid;
			/* now close off the useless stuff, and read the goodies */
			if (close(nodeptr->out.fds[1]) != 0)
				log_bailout(__LINE__);
			fd = fdopen(nodeptr->out.fds[0], "r"); /* stdout */
			if (fd == NULL)
				log_bailout(__LINE__);
			while ((cd = fgets(pipebuf, sizeof(pipebuf), fd))) {
				if (cd != NULL) {
					nodeptr->index = atof(cd);
					syslog(LOG_DEBUG, "recorded speed %f for node %s",
						nodeptr->index, nodeptr->name);
				}
			}
			fclose(fd);
			(void)wait(&status);
			nodeptr = nodeptr->next;
		} /* for i */			
	} /* for n */
}

/*ARGSUSED*/
void
log_bailout(line) 
	int line;
{
	extern int errno;
	
	if (debug)
		syslog(LOG_CRIT, "%s: Failed on line %d: %m %d", progname, line,
			errno);
	else
		syslog(LOG_CRIT, "%s: Internal error, aborting: %m", progname);

	_exit(EXIT_FAILURE);
}

void
sig_handler(i)
	int i;
{
	extern int gotsigterm, gotsigusr1, gotsigusr2;

	switch (i) {
	case SIGTERM:
		gotsigterm = 1;
		break;
	case SIGUSR1:
		gotsigusr1++;
		break;
	case SIGUSR2:
		gotsigusr2++;
		break;
	default:
		log_bailout(__LINE__);
		break;
	}
}
