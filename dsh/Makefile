# $Id: Makefile,v 1.5 1998/10/15 20:54:57 garbled Exp $
# dsh Makefile: Tim Rightnour

#CC=		/usr/local/bin/gcc
CFLAGS=		-Wall -O2
PROGS=		dsh seq run
OPSYS!=		uname

.if ${OPSYS} != NetBSD
SSRCS= strsep.o
.else
SSRCS= 
.endif

MAN= dsh.1 seq.1 run.c

all: ${PROGS}.o ${SSRCS}
	for prog in ${PROGS} ; do \
		${CC} ${CFLAGS} -o $$prog $$prog.o ${SSRCS:.c=.o}; \
	done

install: ${PROGS}
	/usr/bin/install -c -s -o bin -g bin -m 0755 $@ ${PREFIX}/bin
	/usr/bin/install -c -o bin -g bin -m 0444 $@.1 ${PREFIX}/man/man1

clean:
	rm -f *.o ${PROGS}
