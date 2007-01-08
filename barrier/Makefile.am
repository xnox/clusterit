## Process this file with automake to produce Makefile.in
AUTOMAKE=@AUTOMAKE@ --add-missing --foreign
ACLOCAL=@ACLOCAL@ -I autoconf

INCLUDES = 

bin_PROGRAMS = barrierd barrier

barrier_SOURCES = \
	barrier.c \
	../common/common.c ./common/common.h \
	../common/netbsd.c

barrierd_SOURCES = \
	barrierd.c \
	../common/sockcommon.c ../common/sockcommon.h \
	../common/common.c ./common/common.h \
	../common/netbsd.c

man_MANS = \
	barrier.1 \
	barrierd.1

EXTRA_DIST = $(man_MANS)

barrier_LDADD = 
barrierd_LDADD = 
