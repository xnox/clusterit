# $Id: Makefile,v 1.8 1998/10/20 07:28:43 garbled Exp $
# dsh Makefile: Tim Rightnour

# Some of the below needs to be uncommented for Solaris to work.

#CC=		/usr/local/bin/gcc
CFLAGS=		-Wall -O2
PROGS=		dsh seq run

OBJS= dsh.o seq.o run.o
#SSRCS= strsep.o
SSRCS= 

MAN= dsh.1 seq.1 run.1

all: ${OBJS} ${SSRCS}
	@for prog in ${PROGS} ; do \
		echo ${CC} ${CFLAGS} -o $$prog $$prog.o ${SSRCS}; \
		${CC} ${CFLAGS} -o $$prog $$prog.o ${SSRCS}; \
	done

install: ${PROGS} ${MAN}
	/usr/bin/install -c -s -o bin -g bin -m 0755 ${PROGS} ${PREFIX}/bin
	/usr/bin/install -c -o bin -g bin -m 0444 ${MAN} ${PREFIX}/man/man1

clean:
	rm -f *.o ${PROGS}
