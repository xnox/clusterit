## Process this file with automake to produce Makefile.in
AUTOMAKE=@AUTOMAKE@ --add-missing --foreign
ACLOCAL=@ACLOCAL@ -I autoconf

AM_CPPFLAGS = -DNEED_PTY @X_CFLAGS@

INCLUDES = 

bin_PROGRAMS = rvt

rvt_SOURCES = \
	rvt.c \
	command.c \
	sbar.c \
	screen.c \
	xsetup.c \
	ttyinit.c \
	../common/common.c \
	../common/netbsd.c

man_MANS = \
	rvt.1

rvt_LDADD = @X_LIBS@ -lX11 @ADDLIBS@
