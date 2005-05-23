/* $Id: sockcommon.h,v 1.8 2005/05/23 05:34:42 garbled Exp $ */
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

/* Headers for sockcommon.c, used by jsd, jsh, barrier. */

#include "config.h"

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <sys/socket.h>
#ifdef HAVE_SYS_MBUF_H
#include <sys/mbuf.h>
#endif
#include <net/if.h>
#ifdef HAVE_NET_IF_DL_H
#include <net/if_dl.h>
#endif
#ifdef HAVE_NETINET_IF_ETHER_H
#include <netinet/if_ether.h>
#endif
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/uio.h>
#include <sys/socket.h>

#define MAXMSG		512
#define JSDIPORT	2001
#define JSDOPORT	2002
#define BARRIER_SOCK	1933	/* default socket for barrier */

int make_socket(int port);
int write_to_client(int filedes, const char *buf);
int read_from_client(int filedes, char **j);
extern void _log_bailout(int line, char *file);

#define log_bailout() _log_bailout(__LINE__, __FILE__)
