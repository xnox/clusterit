# $Id: Makefile,v 1.4 2000/02/17 07:31:30 garbled Exp $
# barrier Makefile: Tim Rightnour

# Some options need to be uncommented for Solaris

#CC=		/usr/local/bin/gcc
CFLAGS?=	-Wall -O2
PROGS= 		barrierd barrier
#LIBS=		-lsocket -lnsl
#
BSRCS=		barrier.c ../common/sockcommon.c
DSRCS=		barrierd.c ../common/sockcommon.c
#DSRCS+=		strsep.c

MAN= barrierd.1 barrier.1

all: barrier barrierd

barrier: ${BSRCS:.c=.o}
	${CC} ${CFLAGS} -o $@ ${BSRCS:.c=.o} ${LIBS}

barrierd: ${DSRCS:.c=.o}
	${CC} ${CFLAGS} -o $@ ${DSRCS:.c=.o} ${LIBS}

install: ${PROGS}
	/usr/bin/install -c -s -o bin -g bin -m 0755 barrier ${PREFIX}/bin
	/usr/bin/install -c -o bin -g bin -m 0444 barrier.1 ${PREFIX}/man/man1
	/usr/bin/install -c -s -o bin -g bin -m 0755 barrierd ${PREFIX}/bin
	/usr/bin/install -c -o bin -g bin -m 0444 barrierd.1 ${PREFIX}/man/man1

clean:
	rm -f *.o ${PROGS}
