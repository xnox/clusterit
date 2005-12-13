/* $Id: barrier.c,v 1.19 2005/12/13 05:01:54 garbled Exp $ */
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

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>

#include "../common/common.h"
#include "../common/sockcommon.h"

#if !defined(lint) && defined(__NetBSD__)
__COPYRIGHT(
"@(#) Copyright (c) 1998, 1999, 2000\n\
        Tim Rightnour.  All rights reserved\n");
__RCSID("$Id: barrier.c,v 1.19 2005/12/13 05:01:54 garbled Exp $");
#endif

int quietflag, barrier_port, debug;
char *barrier_host;
char *progname;

int make_barrier(char *key, int nodes);
int write_to_server(int filedes, char *buf);
void _log_bailout(int line, char *file);

int
main(int argc, char **argv)
{
    extern char *optarg;
    extern char *version;

    char *key, *p, *q;
    int nodes, ch, code;
	
    barrier_port = BARRIER_SOCK;
    barrier_host = NULL;
    quietflag = 1;
    key = NULL;
    nodes = 0;

    progname = p = q = strdup(argv[0]);
    while (progname != NULL) {
	q = progname;
	progname = (char *)strsep(&p, "/");
    }
    progname = q;

#if defined(__linux__)
    while ((ch = getopt(argc, argv, "+?dh:k:p:s:v")) != -1)
#else
    while ((ch = getopt(argc, argv, "?dh:k:p:s:v")) != -1)
#endif
	switch (ch) {
	case 'q':
	    quietflag = 0;
	    break;
	case 'h':
	    barrier_host = strdup(optarg);
	    break;
	case 'k':
	    key = strdup(optarg);
	    break;
	case 'p':
	    barrier_port = atoi(optarg);
	    break;
	case 's':
	    nodes = atoi(optarg);
	    break;
	case '?':
	    (void)fprintf(stderr, "usage: barrier [-qv] [-h barrier_host]"
			  " [-k key] [-p port] -s cluster_size\n");
	    exit(EXIT_FAILURE);
	    break;
	case 'v':
	    (void)printf("%s: %s\n", progname, version);
	    exit(EXIT_SUCCESS);
	    break;
	default:
	    break;
	}
    if (nodes < 1) {
	(void)fprintf(stderr, "usage: barrier [-q] [-h barrier_host] "
	    "[-k key] [-p port] -s cluster_size\n");
	exit(EXIT_FAILURE);
    }
    if (key == NULL)
	key = strdup("barrier");
    if (key == NULL)
	bailout();

    if (barrier_port == BARRIER_SOCK)
	if (getenv("BARRIER_PORT") != NULL)
	    barrier_port = atoi(getenv("BARRIER_PORT"));

    if (barrier_host == NULL) {
	if (getenv("BARRIER_HOST") != NULL)
	    barrier_host = strdup(getenv("BARRIER_HOST"));
	else {
	    (void)fprintf(stderr, "No barrier host given on command line,"
		" and BARRIER_HOST environment not found.\n");
	    exit(EXIT_FAILURE);
	}
    }

    code = make_barrier(key, nodes);
    if (code == 6) {
	if (!quietflag)
	    (void)printf("Barrier met, continuing: %s\n", key);
	return(EXIT_SUCCESS);
    } else
	if (!quietflag)
	    (void)printf("Barrier encountered a server error, aborting.\n");
    return(EXIT_FAILURE); 
}

int make_barrier(char *key, int nodes)
{
    int sock;
    char *p;
    char message[256];
    struct sockaddr_in name;
    struct hostent *hostinfo;

    /* create socket */
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
	perror("socket");
	exit(EXIT_FAILURE);
    }

    name.sin_family = AF_INET;
    name.sin_port = htons(barrier_port);
    hostinfo = gethostbyname(barrier_host);

    if (hostinfo == NULL) {
	(void)fprintf(stderr, "Unknown host %s.\n", barrier_host);
	exit(EXIT_FAILURE);
    }

    name.sin_addr = *(struct in_addr *) hostinfo->h_addr;

    if (connect (sock, (struct sockaddr *) &name, sizeof (name)) < 0) {
	perror ("connect");
	exit (EXIT_FAILURE);
    }
    if (!quietflag)
	(void)printf("Barrier syncing with token: %s\n", key);
    (void)snprintf(message, 256, "%s %d", key, nodes);
    write_to_server(sock, message);
    return(read(sock, &p, 6));
}

int write_to_server(int filedes, char *buf)
{
    int nbytes;

    nbytes = write(filedes, buf, strlen(buf));
    if (nbytes < 0) {
	return(EXIT_FAILURE);
    }
    return(0);
}

void
_log_bailout(int line, char *file)
{
    _bailout(line, file);
}
