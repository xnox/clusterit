# $Id: Makefile,v 1.6 2001/08/14 04:10:09 garbled Exp $
# jsd Makefile: Tim Rightnour

# Some of the below needs to be uncommented for Solaris to work.

#CC=		/usr/vac/bin/cc
#CC=		/usr/local/bin/gcc
CFLAGS?=	-O2 -g #-DDEBUG -g
CFLAGS+=	-DCLUSTERS
LINTFLAGS+=	-DCLUSTERS
PROGS=		jsd jsh

OBJS= jsh.o jsd.o
#SSRCS= ../common/netbsd.o ../common/common.o ../common/sockcommon.o
SSRCS= ../common/common.o ../common/sockcommon.o
#LIBS= -L/usr/pkg/lib -lefence  # this is for my debugging only.

MAN=	jsh.1 jsd.1

all: ${OBJS} ${SSRCS} ../common/common.h ../common/sockcommon.h
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
	${INSTALL} -c -o root -g wheel -m 0444 ${MAN} ${PREFIX}/man/man1

clean:
	rm -f *.o *.core core *~ ${PROGS} ../common/*.o ../common/*.ln *.ln
