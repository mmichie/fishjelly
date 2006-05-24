#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(shelob, 0.5, mmichie@daimyo.org)
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([webserver.cc])
AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lnsl':
AC_CHECK_LIB([nsl], [main])
# FIXME: Replace `main' with a function in `-lsocket':
AC_CHECK_LIB([socket], [main])

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([arpa/inet.h netinet/in.h strings.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_STRUCT_TM

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MKTIME
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([inet_ntoa socket strstr])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
