## Process this file with automake to produce Makefile.in
AUTOMAKE=@AUTOMAKE@ --add-missing --foreign
ACLOCAL=@ACLOCAL@ -I autoconf

AM_CPPFLAGS = -DCLUSTERS

INCLUDES = 

bin_PROGRAMS = jsh jsd

jsd_SOURCES = \
	jsd.c \
	../common/common.c ../common/common.h \
	../common/sockcommon.c ../common/sockcommon.h \
	../common/netbsd.c

jsh_SOURCES = \
	jsh.c \
	../common/common.c ../common/common.h \
	../common/sockcommon.c ../common/sockcommon.h \
	../common/netbsd.c

man_MANS = \
	jsh.1 \
	jsd.1

jsh_LDADD =
jsd_LDADD =