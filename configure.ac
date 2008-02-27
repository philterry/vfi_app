#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([testprog], [0.1], [Phil Terry <pterry@vmetro.com>])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/testprog.c])
AC_CONFIG_HEADER([config.h])
AC_PROG_LIBTOOL

# Checks for programs.
AC_PROG_CC
AC_CHECK_PROG([GGO],[gengetopt],[gengetopt])

MMC_PATH_RDDMA_API

# Checks for libraries.
AC_CHECK_LIB([rddma_api], [rddma_open])
AC_CHECK_LIB([pthread],[sem_init])

# Checks for header files.
AC_CHECK_HEADERS([unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_C_CONST
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_HEADER_STDC
AC_FUNC_MMAP
AC_CHECK_FUNCS([memset strstr strtol])

AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT
