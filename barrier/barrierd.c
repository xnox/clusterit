/* $Id: barrierd.c,v 1.4 1998/10/20 07:26:34 garbled Exp $ */
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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>

#if !defined(lint) && defined(__NetBSD__)
__COPYRIGHT(
"@(#) Copyright (c) 1998\n\
        Tim Rightnour.  All rights reserved\n");
#endif /* not lint */

#if !defined(lint) && defined(__NetBSD__)
__RCSID("$Id: barrierd.c,v 1.4 1998/10/20 07:26:34 garbled Exp $");
#endif

#define BARRIER_SOCK	1933	/* default socket for barrier */
#define MAXMSG		512
#define MAX_TOKENS	10
#define MAX_CLUSTER	512

int barrier_port;

#ifdef __NetBSD__
int make_socket __P((void));
int sleeper __P((void));
int write_to_client __P((int filedes, char *buf));
int read_from_client __P((int filedes, char **j));
#else
int make_socket(void);
int sleeper(void);
int write_to_client(int filedes, char *buf);
int read_from_client(int filedes, char **j);
#endif

int main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;

	int ch;
	
	barrier_port = BARRIER_SOCK;

	while ((ch = getopt(argc, argv, "?p:")) != -1)
		switch (ch) {
		case 'p':
			barrier_port = atoi(optarg);
			break;
		case '?':
			(void)fprintf(stderr, "usage: barrierd [-p port]\n");
			exit(EXIT_FAILURE);
			break;
		default:
			break;
	}

	if (barrier_port == BARRIER_SOCK)
		if (getenv("BARRIER_PORT") != NULL)
			barrier_port = atoi(getenv("BARRIER_PORT"));

	return(sleeper());
}

int make_socket(void)
{
	int sock;
	struct sockaddr_in name;

	/* create socket */
	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	name.sin_family = AF_INET;
	name.sin_port = htons(barrier_port);
	name.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(sock, (struct sockaddr *) &name, sizeof(name)) != 0) {
		perror("bind");
		exit(EXIT_FAILURE);
	}
	return(sock);
}

int write_to_client(int filedes, char *buf)
{
	int nbytes;

	nbytes = write(filedes, buf, strlen(buf));
	if (nbytes < 0)
		return(EXIT_FAILURE);
	else
		return(EXIT_SUCCESS);
}

int read_from_client(int filedes, char **j)
{
	int nbytes;
	char *buffer;

	buffer = malloc( MAXMSG * sizeof(char));

	nbytes = read(filedes, buffer, MAXMSG);
	if (nbytes < 0) {
		/* Read error. */
		perror("read");
		exit (EXIT_FAILURE);
	} else if (nbytes == 0)
		/* End-of-file. */
		return(-1);
	else { /* Data read. */
		/* place data from the socket into the buffer we were passed */
		*j = strdup(buffer);
		return(nbytes);
	}
	/*NOTREACHED*/
}

int sleeper(void)
{
	fd_set active_fd_set, read_fd_set;
	struct sockaddr_in clientname;
	char *key, *buf;
	char *tokens[MAX_TOKENS];
	int i, k, l, m, found, size, sock;
	int sizes[MAX_TOKENS];
	int connections[MAX_TOKENS];
	int sockets[MAX_TOKENS][MAX_CLUSTER];

#ifndef DEBUG
	switch (fork()) {
	case 0: 
#endif
		/* Create the socket and set it up to accept connections. */
		sock = make_socket();

		if (listen(sock, 1) < 0) {
			perror("listen");
			exit(EXIT_FAILURE);
		}
		for (l=0; l < MAX_TOKENS; l++) {
			connections[l] = 0;
			tokens[l] = NULL;
			for (m=0; m < MAX_CLUSTER; m++)
				sockets[l][m] = 0;
		}	

		/* Initialize the set of active sockets. */
		FD_ZERO(&active_fd_set);
		FD_SET(sock, &active_fd_set);
#ifdef DEBUG
		printf("Server going into wait mode, listening on port %d\n", barrier_port);
#endif
		while (1) {
			/* Block until input arrives on one or more active sockets. */
			read_fd_set = active_fd_set;
			if (select(FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0) {
				perror ("select");
				exit (EXIT_FAILURE);
			}
			/* Service all the sockets with input pending. */
			for (i = 0; i < FD_SETSIZE; ++i)
				if (FD_ISSET (i, &read_fd_set)) {
					if (i == sock) {
						/* Connection request on original socket. */
						int new;
						size = sizeof(clientname);
						new = accept(sock, (struct sockaddr *) &clientname, &size);
						if (new < 0) {
							perror("accept");
							exit(EXIT_FAILURE);
						}
#ifdef DEBUG
						(void)fprintf(stderr, "Server: connect from host %s, port %hd.\n",
						    inet_ntoa(clientname.sin_addr), ntohs(clientname.sin_port));
#endif
						FD_SET(new, &active_fd_set);
						if (read_from_client(new, &buf) > 0) {
							key = (char *)strsep(&buf, " ");
							found = 0;
							for (k=0; (found == 0 && k < MAX_TOKENS); ) {
								if (tokens[k] != NULL)
									if (strcmp(tokens[k], key) == 0)
										found = 1;
								if (!found)
									k++;
							}
							if (!found) /* we didn't find a matching token, now make a new one */
							for (k=0; (k < MAX_TOKENS && tokens[k] != NULL); k++)
								;

							if (k > MAX_TOKENS - 1) {
								(void)fprintf(stderr, "Server: too many tokens. Disconnected host %s, port %hd.\n",
								    inet_ntoa(clientname.sin_addr), ntohs(clientname.sin_port));
								write_to_client(new,"fail");
								close(new);
								FD_CLR(new, &active_fd_set);
							} else { 
								for (l=0; sockets[k][l] != 0; l++)
									;
								if (l > MAX_CLUSTER - 1) {
									(void)fprintf(stderr, "Server: too many sockets on token. Disconnected host %s, port %hd.\n",
									    inet_ntoa(clientname.sin_addr), ntohs(clientname.sin_port));
									write_to_client(new,"fail");
									close(new);
									FD_CLR(new, &active_fd_set);
								} else {
									sockets[k][l] = new;
									tokens[k] = key;
									sizes[k] = atoi(buf);
									connections[k] += 1;
									if (connections[k] == sizes[k]) {
										l = connections[k];
										for (m=0; m < l; m++) {
											write_to_client(sockets[k][m],"passed");
											close(sockets[k][m]);
											FD_CLR (sockets[k][m], &active_fd_set);
											sockets[k][m] = 0;
											tokens[k] = NULL;
											sizes[k] = 0;
											connections[k] = 0;
										} /* for */
									} /* connections == sizes */
								} /* too many sockets */
							} /* too many tokens */
						} /* data from client */
					} /* i == sock */
				} /* fd isset */
		} /* infinate while */
#ifndef DEBUG
	default:
		exit(EXIT_SUCCESS);
		break;
	} /* switch */
#endif
}
