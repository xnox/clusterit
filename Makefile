# $Id: Makefile,v 1.10 2000/02/25 21:47:11 garbled Exp $
# Makefile for clusterit:  Tim Rightnour

INSTALL=	/usr/bin/install
OPSYS!=		uname
#CC=		/usr/local/bin/gcc
PREFIX?=	/usr/local

SUBDIR=		dsh pcp barrier jsd tools

all:
	@for dir in ${SUBDIR} ; do \
		(cd $$dir && make clean OPSYS=${OPSYS}) ;\
		(cd $$dir && make CC=${CC} OPSYS=${OPSYS}) ;\
	done

clean:
	@for dir in ${SUBDIR} ; do \
		(cd $$dir && make clean OPSYS=${OPSYS}) ;\
	done

install:
	@for dir in ${SUBDIR} ; do \
		(cd $$dir && make install INSTALL=${INSTALL} OPSYS=${OPSYS} \
		 PREFIX=${PREFIX}) ;\
	done

catmans:
	-@mkdir catman
	for dir in ${SUBDIR} ; do \
		(cd $$dir ; \
		for i in `ls *.1` ; do \
			nroff -man $$i >../catman/`/usr/bin/basename $$i .1`.0 ; \
		done );\
	done

htmlmans:
	-@mkdir html/man
	for dir in ${SUBDIR} ; do \
		(cd $$dir ; \
		for i in `ls *.1` ; do \
			groff -mdoc2html -dformat=HTML -P-b -P-u -P-o -Tascii \
			-ww $$i >../html/man/`/usr/bin/basename $$i .1`.html ; \
		done );\
	done
