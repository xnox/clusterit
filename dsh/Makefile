# $Id: Makefile,v 1.13 2000/02/17 07:57:08 garbled Exp $
# dsh Makefile: Tim Rightnour

# Some of the below needs to be uncommented for Solaris to work.

#CC=		/usr/vac/bin/cc
CC=		/usr/local/bin/gcc
CFLAGS=		-O2 -Wall #-DDEBUG -g
PROGS=		dsh seq run

OBJS= dsh.o seq.o run.o
SSRCS= ../common/strsep.o ../common/common.o
#SSRCS= ../common/common.o
#LIBS= -L/usr/pkg/lib -lefence  # this is for my debugging only.

MAN= dsh.1 seq.1 run.1

all: depend ${OBJS} ${SSRCS} ../common/common.h
	@for prog in ${PROGS} ; do \
		echo ${CC} ${CFLAGS} -o $$prog $$prog.o ${SSRCS} ${LIBS}; \
		${CC} ${CFLAGS} -o $$prog $$prog.o ${SSRCS} ${LIBS}; \
	done

${SSRCS}: ${SSRCS:.o=.c}
	${CC} ${CFLAGS} -c $*.c -o $*.o

install: ${PROGS} ${MAN}
	/usr/bin/install -c -s -o bin -g bin -m 0755 ${PROGS} ${PREFIX}/bin
	/usr/bin/install -c -o bin -g bin -m 0444 ${MAN} ${PREFIX}/man/man1

clean:
	rm -f *.o ${PROGS} ../common/*.o
