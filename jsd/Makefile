# $Id: Makefile,v 1.3 2000/02/20 04:46:11 garbled Exp $
# jsd Makefile: Tim Rightnour

# Some of the below needs to be uncommented for Solaris to work.

#CC=		/usr/vac/bin/cc
#CC=		/usr/local/bin/gcc
CFLAGS?=	-O2 -g #-DDEBUG -g
CFLAGS+=	-DCLUSTERS
PROGS=		jsd jsh

OBJS= jsh.o jsd.o
#SSRCS= ../common/strsep.o ../common/common.o ../common/sockcommon.o
SSRCS= ../common/common.o ../common/sockcommon.o
#LIBS= -L/usr/pkg/lib -lefence  # this is for my debugging only.

MAN=	jsh.1 jsd.1

all: ${OBJS} ${SSRCS} ../common/common.h ../common/sockcommon.h
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
