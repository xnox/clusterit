# $Id: Makefile,v 1.16 2001/08/13 21:06:35 garbled Exp $
# dsh Makefile: Tim Rightnour

# Some of the below needs to be uncommented for Solaris to work.

#CC=		/usr/vac/bin/cc
#CC=		/usr/local/bin/gcc
CFLAGS?=	-O2 -Wall #-DDEBUG -g
CFLAGS+=	-DCLUSTERS
LINTFLAGS+=	-DCLUSTERS
#CFLAGS=		-DCLUSTERS
PROGS=		dsh seq run

OBJS= dsh.o seq.o run.o
#SSRCS= ../common/netbsd.o ../common/common.o
SSRCS= ../common/common.o
#LIBS= -L/usr/pkg/lib -lefence  # this is for my debugging only.

MAN= dsh.1 seq.1 run.1

all: ${OBJS} ${SSRCS} ../common/common.h
	@for prog in ${PROGS} ; do \
		echo ${CC} ${CFLAGS} -o $$prog $$prog.o ${SSRCS} ${LIBS}; \
		${CC} ${CFLAGS} -o $$prog $$prog.o ${SSRCS} ${LIBS}; \
	done

lint: ${SSRCS:.o=.ln} ${OBJS:.o=.ln}
	@for prog in ${PROGS} ; do \
		echo ${LINT} ${LINTFLAGS} ${SSRCS:.o=.ln} $$prog.ln ; \
		${LINT} ${LINTFLAGS} ${SSRCS:.o=.ln} $$prog.ln ; \
	done

${SSRCS:.o=.ln} ${OBJS:.o=.ln}: ${SSRCS:.o=.c} ${OBJS:.o=.c}
	${LINT} ${LINTFLAGS} -o $*.ln -i $*.c

${SSRCS}: ${SSRCS:.o=.c}
	${CC} ${CFLAGS} -c $*.c -o $*.o

install: ${PROGS} ${MAN}
	${INSTALL} -c -s -o bin -g bin -m 0755 ${PROGS} ${PREFIX}/bin
	${INSTALL} -c -o bin -g bin -m 0444 ${MAN} ${PREFIX}/man/man1

clean:
	rm -f *.ln *.o *.core core *~ ${PROGS} ../common/*.o ../common/*.ln
