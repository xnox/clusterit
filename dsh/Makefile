# $Id: Makefile,v 1.2 1998/10/13 07:03:24 garbled Exp $
# dsh Makefile: Tim Rightnour

CC=	/usr/local/bin/gcc
CFLAGS=	-Wall -O2
PROG= dsh

.if ${OPSYS} != NetBSD
SRCS= dsh.c strsep.c
.else
SRCS= dsh.c
.endif

MAN= dsh.1

all: dsh

${PROG}: ${SRCS:.c=.o}
	${CC} ${CFLAGS} -o $@ ${SRCS:.c=.o}

clean:
	rm -f *.o ${PROG}
