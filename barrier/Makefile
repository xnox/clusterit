# $Id: Makefile,v 1.2 1998/10/20 07:26:33 garbled Exp $
# barrier Makefile: Tim Rightnour

CC=		/usr/local/bin/gcc
CFLAGS?=	-Wall -O2
PROGS= 		barrierd barrier
LIBS=		-lsocket -lnsl
#
BSRCS=		barrier.c
DSRCS=		barrierd.c
DSRCS+=		strsep.c

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
