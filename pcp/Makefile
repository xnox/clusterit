# $Id: Makefile,v 1.2 1998/10/13 19:59:31 garbled Exp $
# Makefile for dsh:  Tim Rightnour

PROG=		pcp

.if (${OPSYS} != NetBSD)
SRCS= pcp.c strsep.c
.else
SRCS= pcp.c
.endif

SCRIPTS=	prm pdf dshbak
MAN=		pcp.1 prm.1 pdf.1

all: ${PROG} scripts

${PROG}: ${SRCS:.c=.o}
	${CC} -O2 -Wall -o $@ ${SRCS:.c=.o}

scripts: ${SCRIPTS}

install: ${PROG} ${SCRIPTS}
	/usr/bin/install -c -s -o bin -g bin -m 0755 pcp ${PREFIX}/bin
	/usr/bin/install -c -o bin -g bin -m 0755 pdf ${PREFIX}/bin
	/usr/bin/install -c -o bin -g bin -m 0755 prm ${PREFIX}/bin
	/usr/bin/install -c -o bin -g bin -m 0755 dshbak ${PREFIX}/bin
	/usr/bin/install -c -o bin -g bin -m 0444 pcp.1 ${PREFIX}/man/man1
	/usr/bin/install -c -o bin -g bin -m 0444 pdf.1 ${PREFIX}/man/man1
	/usr/bin/install -c -o bin -g bin -m 0444 prm.1 ${PREFIX}/man/man1
	/usr/bin/install -c -o bin -g bin -m 0444 dshbak.1 ${PREFIX}/man/man1

clean:
	rm -f ${PROG} ${SCRIPTS} *.o
