# $Id: Makefile,v 1.2 2001/08/14 04:10:10 garbled Exp $
# rvt Makefile: Tim Rightnour

# Some of the below needs to be uncommented for Solaris to work.

#CC=		/usr/vac/bin/cc
#CC=		/usr/local/bin/gcc
CFLAGS?=	-O2 -Wall #-DDEBUG -g
CFLAGS+=	-I/usr/X11R6/include
LINTFLAGS+=	-I/usr/X11R6/include
#CFLAGS=		-O2 -DCLUSTERS -DNEED_PTY
LDFLAGS=	-L/usr/X11R6/lib -Wl,-R/usr/X11R6/lib
PROGS=		rvt

OBJS= command.o rvt.o sbar.o screen.o xsetup.o
#SSRCS= ../common/netbsd.o
LIBS= -lX11 -lXau -lutil
#LIBS+= -L/usr/pkg/lib -lefence  # this is for my debugging only.

MAN= rvt.1

all: ${OBJS} ${SSRCS} ../common/common.h
	@for prog in ${PROGS} ; do \
		echo ${CC} ${CFLAGS} ${LDFLAGS} -o $$prog ${OBJS} ${SSRCS} ${LIBS}; \
		${CC} ${CFLAGS} ${LDFLAGS} -o $$prog ${OBJS} ${SSRCS} ${LIBS}; \
	done

lint: ${SSRCS:.o=.ln} ${OBJS:.o=.ln}
	@for prog in ${PROGS} ; do \
		echo ${LINT} ${LINTFLAGS} ${SSRCS:.o=.ln} $$prog.ln ; \
		${LINT} ${LINTFLAGS} -lutil ${SSRCS:.o=.ln} $$prog.ln ; \
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
