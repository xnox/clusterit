/* $Id: sockcommon.c,v 1.2 2000/02/17 07:55:07 garbled Exp $ */
/*
 * Copyright (c) 2000
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
 * these are routines that are used in all the jsd and barrier programs,
 * and therefore rather than having to fix them in n places, they are kept
 * here.
 */

#include "../common/sockcommon.h"

#if !defined(lint) && defined(__NetBSD__)
__COPYRIGHT(
"@(#) Copyright (c) 2000\n\
        Tim Rightnour.  All rights reserved\n");
__RCSID("$Id: sockcommon.c,v 1.2 2000/02/17 07:55:07 garbled Exp $");
#endif


int make_socket(port)
	int port;
{
	int sock;
	struct sockaddr_in name;

	/* create socket */
	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0)
		log_bailout(__LINE__);

	name.sin_family = AF_INET;
	name.sin_port = htons(port);
	name.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(sock, (struct sockaddr *) &name, sizeof(name)) != 0)
		log_bailout(__LINE__);

	return(sock);
}

int write_to_client(filedes, buf)
	int filedes;
	char *buf;
{
	int nbytes;

	nbytes = write(filedes, buf, strlen(buf));
	if (nbytes < 0)
		return(EXIT_FAILURE);
	else
		return(EXIT_SUCCESS);
}

int read_from_client(filedes, j)
	int filedes;
	char **j;
{
	int nbytes;
	char *buffer;

	buffer = (char *)malloc( MAXMSG * sizeof(char));

	nbytes = read(filedes, buffer, MAXMSG);
	if (nbytes < 0)
		log_bailout(__LINE__);
	else if (nbytes == 0)
		/* End-of-file. */
		return(-1);
	else { /* Data read. */
		/* place data from the socket into the buffer we were passed */
		*j = strdup(buffer);
		return(nbytes);
	}
	/*NOTREACHED*/
	return(0);
}
