# $Id: Makefile,v 1.5 1998/10/20 07:39:04 garbled Exp $
# Makefile for clusterit:  Tim Rightnour

OPSYS!=		uname
#CC=		/usr/local/bin/gcc
CFLAGS=		-O2 -Wall
PREFIX?=	/usr/local

SUBDIR=		dsh pcp barrier

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
