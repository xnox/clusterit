## Process this file with automake to produce Makefile.in
AUTOMAKE=@AUTOMAKE@ --add-missing --foreign
ACLOCAL=@ACLOCAL@ -I autoconf

AM_CPPFLAGS = -DCLUSTERS

INCLUDES = 

bin_PROGRAMS = pcp

pcp_SOURCES = \
	pcp.c \
	../common/common.c ../common/common.h \
	../common/netbsd.c

man_MANS = \
	pcp.1

pcp_LDADD =
