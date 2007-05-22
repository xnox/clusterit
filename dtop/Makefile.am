## Process this file with automake to produce Makefile.in
AUTOMAKE=@AUTOMAKE@ --add-missing --foreign
ACLOCAL=@ACLOCAL@ -I autoconf

AM_CPPFLAGS = -DCLUSTERS

INCLUDES = 

bin_PROGRAMS = dtop

dtop_SOURCES = \
	dtop.c \
	../common/common.c ../common/common.h \
	../common/netbsd.c

man_MANS = \
	dtop.1

dtop_LDADD = @CURSESLIBS@
