# $Id: Makefile,v 1.6 2000/02/17 08:09:54 garbled Exp $
# Makefile for clusterit:  Tim Rightnour

OPSYS!=		uname
#CC=		/usr/local/bin/gcc
CFLAGS=		-O2 -Wall
PREFIX?=	/usr/local

SUBDIR=		dsh pcp barrier jsd

all:
	@for dir in ${SUBDIR} ; do \
		(cd $$dir && make CC=${CC} "CFLAGS=${CFLAGS}" OPSYS=${OPSYS}) ;\
	done

clean:
	@for dir in ${SUBDIR} ; do \
		(cd $$dir && make clean OPSYS=${OPSYS}) ;\
	done

install:
	@for dir in ${SUBDIR} ; do \
		(cd $$dir && make install OPSYS=${OPSYS} PREFIX=${PREFIX}) ;\
	done
