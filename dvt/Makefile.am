## Process this file with automake to produce Makefile.in
AUTOMAKE=@AUTOMAKE@ --add-missing --foreign
ACLOCAL=@ACLOCAL@ -I autoconf

AM_CPPFLAGS = -DCLUSTERS -DUSE_X11 @X_CFLAGS@

INCLUDES = 

bin_PROGRAMS = dvt

dvt_SOURCES = \
	dvt.c \
	../common/common.c ../common/common.h \
	../common/netbsd.c

man_MANS = \
	dvt.1

dvt_LDADD = @X_LIBS@ -lX11
