# $Id: Makefile,v 1.7 1998/10/16 00:58:02 garbled Exp $
# dsh Makefile: Tim Rightnour

#CC=		/usr/local/bin/gcc
CFLAGS=		-Wall -O2
PROGS=		dsh seq run
OPSYS!=		uname

.if ${OPSYS} != NetBSD
SSRCS= strsep.c
.else
SSRCS= 
.endif

MAN= dsh.1 seq.1 run.1

all: ${PROGS}.c ${SSRCS}
	for prog in ${PROGS} ; do \
		${CC} ${CFLAGS} -o $$prog $$prog.c ${SSRCS}; \
	done

install: ${PROGS} ${MAN}
	/usr/bin/install -c -s -o bin -g bin -m 0755 ${PROGS} ${PREFIX}/bin
	/usr/bin/install -c -o bin -g bin -m 0444 ${MAN} ${PREFIX}/man/man1

clean:
	rm -f *.o ${PROGS}
