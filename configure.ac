#
# bgpstream
#
# Chiara Orsini, CAIDA, UC San Diego
# chiara@caida.org
#
# Copyright (C) 2013 The Regents of the University of California.
#
# This file is part of bgpstream.
#
# bgpstream is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# bgpstream is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with bgpstream.  If not, see <http://www.gnu.org/licenses/>.
#

AC_PREREQ([2.68])

AC_INIT([bgpstream], [0.0.0], [chiara@caida.org])

BGPSTREAM_MAJOR_VERSION=0
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
fi

AM_CONDITIONAL([WITH_WANDIO], [test "x$with_wandio" == xyes])


# we may want to come back later and add compile-time configuration for things
# like datastructure providers, but for now it will all get compiled


AC_SUBST([BGPSTREAM_MAJOR_VERSION])
AC_SUBST([BGPSTREAM_MID_VERSION])
AC_SUBST([BGPSTREAM_MINOR_VERSION])

AC_HEADER_ASSERT

AC_CONFIG_FILES([Makefile
		lib/Makefile
		tools/Makefile
		common/Makefile
		common/libpatricia/Makefile
		common/libcsv/Makefile
		bgpcommon/Makefile
                bgpdump-caida/Makefile])
AC_OUTPUT
