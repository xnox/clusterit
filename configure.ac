#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(clusterit, 2.4, root@garbled.net)
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([dsh/dsh.c])
AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PATH_PROGS(AWK, gawk /usr/xpg4/bin/awk awk)
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

AC_CHECK_LIB([util], [openpty], [AC_DEFINE(HAVE_OPENPTY)
	ADDLIBS="$ADDLIBS -lutil"])
AC_CHECK_LIB([util], [login_tty], [AC_DEFINE(HAVE_LOGIN_TTY)
	ADDLIBS="$ADDLIBS -lutil"])
AC_CHECK_LIB([socket], [listen])
AC_CHECK_LIB([nsl], [gethostbyname])

# Checks for header files.
AC_PATH_XTRA
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([sys/param.h sys/mbuf.h net/if_dl.h netinet/if_ether.h sys/cdefs.h util.h termios.h],,,
[[#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
]])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_UID_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_HEADER_TIME

# Checks for library functions.
AC_FUNC_CHOWN
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_PROG_GCC_TRADITIONAL
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_SETPGRP
AC_TYPE_SIGNAL
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([strsep setpriority openpty login_tty])

AC_SUBST(X11LIBS)
AC_SUBST(ADDLIBS)

AC_CONFIG_FILES([Makefile
                 barrier/Makefile
                 dsh/Makefile
                 dvt/Makefile
                 jsd/Makefile
                 pcp/Makefile
                 rvt/Makefile
                 tools/Makefile])
AC_OUTPUT
