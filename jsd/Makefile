# $Id: Makefile,v 1.1 2000/01/17 04:02:28 garbled Exp $
# jsd Makefile: Tim Rightnour

# Some of the below needs to be uncommented for Solaris to work.

CC=		/usr/vac/bin/cc
#CC=		/usr/local/bin/gcc
CFLAGS=		-O2 -g #-DDEBUG -g
PROGS=		jsd jsh

OBJS= jsh.o jsd.o
SSRCS= strsep.o ../dsh/common.o jcommon.o
#SSRCS= ../dsh/common.o jcommon.o
#LIBS= -L/usr/pkg/lib -lefence  # this is for my debugging only.

MAN= #empty

all: ${OBJS} ${SSRCS} ../dsh/common.h jcommon.h
	@for prog in ${PROGS} ; do \
		echo ${CC} ${CFLAGS} -o $$prog $$prog.o ${SSRCS} ${LIBS}; \
		${CC} ${CFLAGS} -o $$prog $$prog.o ${SSRCS} ${LIBS}; \
	done

install: ${PROGS} ${MAN}
	/usr/bin/install -c -s -o bin -g bin -m 0755 ${PROGS} ${PREFIX}/bin
	/usr/bin/install -c -o bin -g bin -m 0444 ${MAN} ${PREFIX}/man/man1

clean:
	rm -f *.o ${PROGS}
