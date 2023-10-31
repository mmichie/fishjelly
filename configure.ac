AC_PREREQ(2.59)
AC_INIT([fishjelly], [0.6], [mmichie@gmail.com])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])
# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_AWK

AM_INIT_AUTOMAKE([foreign])

# Checks for header files.
AC_CHECK_HEADERS([dirent.h arpa/inet.h limits.h netdb.h netinet/in.h stdlib.h strings.h sys/socket.h sys/time.h unistd.h sys/wait.h])


# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_CHECK_FUNCS([fork mktime strftime inet_ntoa socket strstr])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT

