# $Id: Makefile,v 1.12 2001/08/13 23:04:13 garbled Exp $
# Makefile for dsh:  Tim Rightnour

#Uncomment things for solaris.

#CC= /usr/local/bin/gcc
#CC= /usr/vac/bin/cc
PROG=		pcp
CFLAGS?=	-O2 -Wall
CFLAGS+=	-DCLUSTERS
#CFLAGS=		-DCLUSTERS

#SRCS= pcp.c ../common/netbsd.c ../common/common.c
SRCS= pcp.c ../common/common.c

MAN=		pcp.1

all: ${PROG}

${PROG}: ${SRCS:.c=.o}
	${CC} ${CFLAGS} -o $@ ${SRCS:.c=.o}

${SRCS:.c=.o}: ${SRCS}
	${CC} ${CFLAGS} -c $*.c -o $*.o

${SRCS:.c=.ln}: ${SRCS}
	${LINT} ${LINTFLAGS} -o $*.ln -i $*.c

lint: ${SRCS:.c=.ln}
	${LINT} ${LINTFLAGS} ${SRCS:.o=.ln}
		
install: ${PROG} ${MAN}
	${INSTALL} -c -s -o bin -g bin -m 0755 pcp ${PREFIX}/bin
	${INSTALL} -c -o bin -g bin -m 0444 pcp.1 ${PREFIX}/man/man1
clean:
	rm -f ${PROG} *~ *.core core *.o ../common/*.o ../common/*.ln *.ln
