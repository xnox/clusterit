# $Id: Makefile,v 1.4 1998/10/15 18:05:04 garbled Exp $
# dsh Makefile: Tim Rightnour

#CC=	/usr/local/bin/gcc
CFLAGS=	-Wall -O2
PROGS= dsh seq

#.if ${OPSYS} != NetBSD
#DSRCS= dsh.c strsep.c
#SSRCS= dsh.c strsep.c
#.else
DSRCS= dsh.c
SSRCS= seq.c
#.endif

MAN= dsh.1 seq.1

all: dsh seq

dsh: ${DSRCS:.c=.o}
	${CC} ${CFLAGS} -o $@ ${DSRCS:.c=.o}

seq: ${SSRCS:.c=.o}
	${CC} ${CFLAGS} -o $@ ${SSRCS:.c=.o}

install: ${PROGS}
	/usr/bin/install -c -s -o bin -g bin -m 0755 dsh ${PREFIX}/bin
	/usr/bin/install -c -o bin -g bin -m 0444 dsh.1 ${PREFIX}/man/man1
	/usr/bin/install -c -s -o bin -g bin -m 0755 seq ${PREFIX}/bin
	/usr/bin/install -c -o bin -g bin -m 0444 seq.1 ${PREFIX}/man/man1

clean:
	rm -f *.o ${PROGS}
