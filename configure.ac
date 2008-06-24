#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([vfi_app], [0.1], [Phil Terry <pterry@vmetro.com>])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/testprog.c])
AC_CONFIG_HEADER([config.h])
AC_PROG_LIBTOOL

# Checks for programs.
AC_PROG_CC
AC_CHECK_PROG([GGO],[gengetopt],[gengetopt])

GTK_DOC_CHECK([1.0])

#Uncomment either the following line or the subsequent two lines
PKG_CHECK_MODULES(VFI_API,libvfi_api libvfi_frmwrk)
#VFI_PATH_VFI_API
#VFI_PATH_VFI_FRMWRK

# Checks for libraries.
AC_CHECK_LIB([pthread],[sem_init])

# Checks for header files.
AC_CHECK_HEADERS([unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_C_CONST
AC_TYPE_SIZE_T

# Checks for library functions.

AC_CONFIG_FILES([Makefile
                 src/Makefile doc/Makefile testsuite/Makefile])
AC_OUTPUT
