# $Id: Makefile,v 1.1 1998/10/13 06:50:47 garbled Exp $
# Makefile for clusterit:  Tim Rightnour

OPSYS!=	uname

CC=	/usr/local/bin/gcc
CFLAGS=	-O2 -Wall

SUBDIR=	dsh pcp

all:
	for dir in ${SUBDIR} ; do \
		(cd $$dir && make CC=${CC} "CFLAGS=${CFLAGS}" OPSYS=${OPSYS}) ;\
	done

clean:
	for dir in ${SUBDIR} ; do \
		(cd $$dir && make clean OPSYS=${OPSYS}) ;\
	done

