# $Id: Makefile,v 1.6 1999/05/05 07:50:24 garbled Exp $
# Makefile for dsh:  Tim Rightnour

#Uncomment things for solaris.

#CC= /usr/local/bin/gcc
PROG=		pcp
CFLAGS?=	-O2 -Wall

#SRCS= pcp.c strsep.c
SRCS= pcp.c

SCRIPTS=	prm pdf dshbak
MAN=		pcp.1 prm.1 pdf.1

all: ${PROG} scripts

${PROG}: ${SRCS:.c=.o}
	${CC} ${CFLAGS} -o $@ ${SRCS:.c=.o}

scripts: ${SCRIPTS}

dshbak.sh: dshbak.src
	sed -e 's|@@@AWK@@@|/usr/bin/awk|' < $> > $@
#	sed -e 's|@@@AWK@@@|/usr/xpg4/bin/awk|' < $> > $@

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
	rm -f ${PROG} ${SCRIPTS} *.o dshbak.sh
