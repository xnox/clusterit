## Process this file with automake to produce Makefile.in
AUTOMAKE=@AUTOMAKE@ --add-missing --foreign
ACLOCAL=@ACLOCAL@ -I autoconf

AM_CPPFLAGS = -DCLUSTERS

INCLUDES = 

bin_PROGRAMS = dsh rseq run

dsh_SOURCES = \
	dsh.c \
	../common/common.c ../common/common.h \
	../common/netbsd.c

rseq_SOURCES = \
	rseq.c \
	../common/common.c ../common/common.h \
	../common/netbsd.c

run_SOURCES = \
	run.c \
	../common/common.c ../common/common.h \
	../common/netbsd.c

man_MANS = \
	dsh.1 \
	rseq.1 \
	run.1

dsh_LDADD = 
rseq_LDADD = 
run_LDADD = 