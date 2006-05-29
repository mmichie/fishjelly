#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
#AC_INIT(shelob, 0.6, mmichie@gmail.com)
AC_INIT(src/webserver.cc)
AC_CONFIG_SRCDIR([config.h.in])
AM_CONFIG_HEADER([config.h])
# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_AWK

AM_INIT_AUTOMAKE(shelob, 0.6)

# Checks for libraries.

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([arpa/inet.h limits.h netdb.h netinet/in.h stdlib.h strings.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_STRUCT_TM

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MKTIME
AC_TYPE_SIGNAL
AC_FUNC_STRFTIME
AC_CHECK_FUNCS([inet_ntoa socket strstr])

AC_OUTPUT([Makefile src/Makefile])
