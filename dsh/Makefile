# $Id: Makefile,v 1.10 1999/10/14 16:50:52 garbled Exp $
# dsh Makefile: Tim Rightnour

# Some of the below needs to be uncommented for Solaris to work.

#CC=		/usr/local/bin/gcc
CFLAGS=		-Wall -O2 #-DDEBUG -g
PROGS=		dsh seq run

OBJS= dsh.o seq.o run.o
#SSRCS= strsep.o
SSRCS=
#LIBS= -L/usr/pkg/lib -lefence  # this is for my debugging only.

MAN= dsh.1 seq.1 run.1

all: ${OBJS} ${SSRCS} common.h
	@for prog in ${PROGS} ; do \
		echo ${CC} ${CFLAGS} -o $$prog $$prog.o ${SSRCS} ${LIBS}; \
		${CC} ${CFLAGS} -o $$prog $$prog.o ${SSRCS} ${LIBS}; \
	done

install: ${PROGS} ${MAN}
	/usr/bin/install -c -s -o bin -g bin -m 0755 ${PROGS} ${PREFIX}/bin
	/usr/bin/install -c -o bin -g bin -m 0444 ${MAN} ${PREFIX}/man/man1

clean:
	rm -f *.o ${PROGS}
