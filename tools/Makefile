# $Id: Makefile,v 1.4 2001/08/14 04:10:10 garbled Exp $
# Makefile for dsh:  Tim Rightnour

PROG=		#empty
SRCS=		#empty

SCRIPTS=	prm pdf clustersed dshbak
MAN=		prm.1 pdf.1 dshbak.1 clustersed.1

all: scripts

scripts: ${SCRIPTS}

dshbak.sh: dshbak.src
	sed -e 's|@@@AWK@@@|/usr/bin/awk|' < $> > $@
#	sed -e 's|@@@AWK@@@|/usr/xpg4/bin/awk|' < $> > $@

install: ${PROG} ${SCRIPTS}
	${INSTALL} -c -o bin -g bin -m 0755 clustersed ${PREFIX}/bin
	${INSTALL} -c -o bin -g bin -m 0755 pdf ${PREFIX}/bin
	${INSTALL} -c -o bin -g bin -m 0755 prm ${PREFIX}/bin
	${INSTALL} -c -o bin -g bin -m 0755 dshbak ${PREFIX}/bin
	${INSTALL} -c -o root -g wheel -m 0444 clustersed.1 ${PREFIX}/man/man1
	${INSTALL} -c -o root -g wheel -m 0444 pdf.1 ${PREFIX}/man/man1
	${INSTALL} -c -o root -g wheel -m 0444 prm.1 ${PREFIX}/man/man1
	${INSTALL} -c -o root -g wheel -m 0444 dshbak.1 ${PREFIX}/man/man1

clean:
	rm -f ${PROG} ${SCRIPTS} *.o *.core core *~ dshbak.sh ../common/*.o
