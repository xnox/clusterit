# $Id: Makefile,v 1.9 2001/08/14 04:10:02 garbled Exp $
# barrier Makefile: Tim Rightnour

# Some options need to be uncommented for Solaris

#CC=		/usr/local/bin/gcc
CFLAGS?=	-Wall -O2
PROGS= 		barrierd barrier
#LIBS=		-lsocket -lnsl
#
SSRCS=		../common/sockcommon.c ../common/common.c
SOBJS=		common.o sockcommon.o
BSRCS=		barrier.c
DSRCS=		barrierd.c
OSRCS=		barrierd.c barrier.c
#DSRCS=		barrierd.c ${SSRCS} ../common/netbsd.c

MAN= barrierd.1 barrier.1

all: barrier barrierd

barrier: ${BSRCS:.c=.o} ${SOBJS}
	${CC} ${CFLAGS} -o $@ ${BSRCS:.c=.o} ${SOBJS} ${LIBS}

barrierd: ${DSRCS:.c=.o} ${SOBJS}
	${CC} ${CFLAGS} -o $@ ${DSRCS:.c=.o} ${SOBJS} ${LIBS}

lint: ${OSRCS:.c=.ln} ${SSRCS:.c=.ln}
	@for prog in ${PROGS} ; do \
		echo ${LINT} ${LINTFLAGS} ${SSRCS:.o=.ln} $$prog.ln ; \
		${LINT} ${LINTFLAGS} ${SSRCS:.o=.ln} $$prog.ln ; \
	done

common.o: ../common/common.c
	${CC} ${CFLAGS} -c $? -o $@

sockcommon.o: ../common/sockcommon.c
	${CC} ${CFLAGS} -c $? -o $@

${SSRCS:.c=.ln} ${OSRCS:.c=.ln}: ${SSRCS} ${OSRCS}
	${LINT} ${LINTFLAGS} -o $*.ln -i $*.c

${BSRCS:.c=.o}: ${BSRCS}
	${CC} ${CFLAGS} -c $*.c -o $*.o

${DSRCS:.c=.o}: ${DSRCS}
	${CC} ${CFLAGS} -c $*.c -o $*.o

install: ${PROGS} ${MAN}
	${INSTALL} -c -s -o bin -g bin -m 0755 barrier ${PREFIX}/bin
	${INSTALL} -c -o root -g wheel -m 0444 barrier.1 ${PREFIX}/man/man1
	${INSTALL} -c -s -o bin -g bin -m 0755 barrierd ${PREFIX}/bin
	${INSTALL} -c -o root -g wheel -m 0444 barrierd.1 ${PREFIX}/man/man1

clean:
	rm -f *.o *.core core *~ ${PROGS} ../common/*.o ../common/*.ln *.ln
