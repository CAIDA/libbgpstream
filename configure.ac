#
# This file is part of bgpstream
#
# Copyright (C) 2015 The Regents of the University of California.
# Authors: Alistair King, Chiara Orsini
#
# All rights reserved.
#
# This code has been developed by CAIDA at UC San Diego.
# For more information, contact bgpstream-info@caida.org
#
# This source code is proprietary to the CAIDA group at UC San Diego and may
# not be redistributed, published or disclosed without prior permission from
# CAIDA.
#
# Report any bugs, questions or comments to bgpstream-info@caida.org
#

AC_PREREQ([2.68])

AC_INIT([bgpstream], [1.0.0], [bgpstream-info@caida.org])

BGPSTREAM_MAJOR_VERSION=1
BGPSTREAM_MID_VERSION=0
BGPSTREAM_MINOR_VERSION=0

AC_DEFINE_UNQUOTED([BGPSTREAM_MAJOR_VERSION],$BGPSTREAM_MAJOR_VERSION,
	[bgpstream major version])
AC_DEFINE_UNQUOTED([BGPSTREAM_MID_VERSION],$BGPSTREAM_MID_VERSION,
	[bgpstream mid version])
AC_DEFINE_UNQUOTED([BGPSTREAM_MINOR_VERSION],$BGPSTREAM_MINOR_VERSION,
	[bgpstream minor version])

LT_INIT

AM_INIT_AUTOMAKE

AC_CONFIG_MACRO_DIR([m4])

AC_CONFIG_SRCDIR([lib/bgpstream.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_LIBTOOL
AC_PROG_CXX
AM_PROG_LIBTOOL
AC_PROG_CC_C99 # C99 is required because of bool

AC_SYS_LARGEFILE

AH_VERBATIM([_GNU_SOURCE],
[/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif])

AC_CHECK_FUNCS([gettimeofday memset strdup strstr strsep strlcpy vasprintf])

# should we dump debug output to stderr and not optmize the build?

AC_MSG_CHECKING([whether to build with debug information])
AC_ARG_ENABLE([debug],
    [AS_HELP_STRING([--enable-debug],
        [enable debug data generation (def=no)])],
    [debugit="$enableval"],
    [debugit=no])
AC_MSG_RESULT([$debugit])

if test x"$debugit" = x"yes"; then
    AC_DEFINE([DEBUG],[],[Debug Mode])
else
    AC_DEFINE([NDEBUG],[],[No-debug Mode])
fi

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC


# Checks for libraries

AC_CHECK_LIB([mysqlclient], [mysql_query], ,
               [AC_MSG_ERROR( [libmysqlclient required ])])

AC_CHECK_LIB([sqlite3], [sqlite3_open_v2], ,
               [AC_MSG_ERROR( [libsqlite3 required ])])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h inttypes.h limits.h math.h stdlib.h string.h \
			      time.h sys/time.h])



# default to turning on wandio support
AC_MSG_CHECKING([whether to use wandio for file i/o])
AC_ARG_WITH([wandio],
	[AS_HELP_STRING([--with-wandio],
	  [use libwandio for file i/o (defaults to no)])],
	  [],
	  [with_wandio=no])

AS_IF([test "x$with_wandio" != xno],
	[
	    AC_DEFINE_UNQUOTED([WITH_WANDIO],[1],
		[Use libwandio for file i/o])
	])

AC_MSG_RESULT([$with_wandio])

if test "x$with_wandio" == xyes; then
   	AC_SEARCH_LIBS([wandio_create], [wandio trace], ,[AC_MSG_ERROR(
		 [libwandio required (included with libtrace since 3.0.15)]
		 )])
else
	AC_CHECK_LIB(z, gzopen, [],
		     AC_DEFINE(DONT_HAVE_GZ, 1, Define if libz not present))
	AC_CHECK_LIB(bz2, BZ2_bzReadOpen, [],
	             AC_DEFINE(DONT_HAVE_BZ2, 1, Define if libbzip2 not present))
        AC_DEFINE(WITHOUT_WANDIO, 1, Define if libwandio not present)
fi

AM_CONDITIONAL([WITH_WANDIO], [test "x$with_wandio" == xyes])
AM_CONDITIONAL([WITHOUT_WANDIO], [test "x$with_wandio" == xno])


# we may want to come back later and add compile-time configuration for things
# like datastructure providers, but for now it will all get compiled


AC_SUBST([BGPSTREAM_MAJOR_VERSION])
AC_SUBST([BGPSTREAM_MID_VERSION])
AC_SUBST([BGPSTREAM_MINOR_VERSION])

AC_HEADER_ASSERT

AC_CONFIG_FILES([Makefile
		lib/Makefile
                lib/bgpdump/Makefile
                lib/datasources/Makefile
                lib/utils/Makefile
		tools/Makefile
		common/Makefile
		common/libpatricia/Makefile
		common/libinterval3/Makefile
		common/libinterval3/rb_tree/Makefile
		common/libcsv/Makefile])
AC_OUTPUT
