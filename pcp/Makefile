# $Id: Makefile,v 1.9 2000/02/25 21:47:59 garbled Exp $
# Makefile for dsh:  Tim Rightnour

#Uncomment things for solaris.

#CC= /usr/local/bin/gcc
PROG=		pcp
CFLAGS?=	-O2 -Wall
CFLAGS+=	-DCLUSTERS

#SRCS= pcp.c ../common/strsep.c ../common/common.c
SRCS= pcp.c ../common/common.c

MAN=		pcp.1

all: ${PROG}

${PROG}: ${SRCS:.c=.o}
	${CC} ${CFLAGS} -o $@ ${SRCS:.c=.o}

${SRCS:.c=.o}: ${SRCS}
	${CC} ${CFLAGS} -c $*.c -o $*.o

install: ${PROG}
	${INSTALL} -c -s -o bin -g bin -m 0755 pcp ${PREFIX}/bin
	${INSTALL} -c -o bin -g bin -m 0444 pcp.1 ${PREFIX}/man/man1

clean:
	rm -f ${PROG} *.o dshbak.sh ../common/*.o
