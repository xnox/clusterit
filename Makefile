# $Id: Makefile,v 1.8 2000/02/20 04:56:00 garbled Exp $
# Makefile for clusterit:  Tim Rightnour

OPSYS!=		uname
#CC=		/usr/local/bin/gcc
PREFIX?=	/usr/local

SUBDIR=		dsh pcp barrier jsd

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
		(cd $$dir && make install OPSYS=${OPSYS} PREFIX=${PREFIX}) ;\
	done

catmans:
	-@mkdir catman
	for dir in ${SUBDIR} ; do \
		(cd $$dir ; \
		for i in `ls *.1` ; do \
			nroff -man $$i >../catman/`/usr/bin/basename $$i .1`.0 ; \
		done );\
	done
