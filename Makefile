# $Id: Makefile,v 1.12 2001/08/13 22:39:37 garbled Exp $
# Makefile for clusterit:  Tim Rightnour

OPSYS!=		uname
#CC=		/usr/local/bin/gcc
#CC=		/usr/vac/bin/cc
PREFIX?=	/usr/local
INSTALL?=	/usr/bin/install

SUBDIR=		dsh pcp barrier jsd rvt dvt tools

all:
	@for dir in ${SUBDIR} ; do \
		(cd $$dir && make clean OPSYS=${OPSYS}) ;\
		(cd $$dir && make CC=${CC} OPSYS=${OPSYS}) ;\
	done

clean:
	@for dir in ${SUBDIR} ; do \
		(cd $$dir && make clean OPSYS=${OPSYS}) ;\
	done

lint:
	@for dir in ${SUBDIR} ; do \
		(cd $$dir && make lint OPSYS=${OPSYS}) ;\
	done

install:
	@for dir in ${SUBDIR} ; do \
		(cd $$dir && make install INSTALL=${INSTALL} \
		 OPSYS=${OPSYS} PREFIX=${PREFIX}) ;\
	done

catmans:
	-@mkdir catman
	for dir in ${SUBDIR} ; do \
		(cd $$dir ; \
		for i in `ls *.1` ; do \
			echo "nroff -man $$i >../catman/`/usr/bin/basename $$i .1`.0" ; \
			nroff -man $$i >../catman/`/usr/bin/basename $$i .1`.0 ; \
		done );\
	done

htmlmans:
	-@mkdir html/man
	for dir in ${SUBDIR} ; do \
		(cd $$dir ; \
		for i in `ls *.1` ; do \
			echo "groff -mdoc2html -dformat=HTML -P-b -P-u -P-o -Tascii \
			-ww $$i >../html/man/`/usr/bin/basename $$i .1`.html" ; \
			groff -mdoc2html -dformat=HTML -P-b -P-u -P-o -Tascii \
			-ww $$i >../html/man/`/usr/bin/basename $$i .1`.html ; \
		done );\
	done
