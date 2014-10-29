#
# bgpwatcher
#
# Alistair King, CAIDA, UC San Diego
# corsaro-info@caida.org
#
# Copyright (C) 2012 The Regents of the University of California.
#
# This file is part of bgpwatcher.
#
# bgpwatcher is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# bgpwatcher is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with bgpwatcher.  If not, see <http://www.gnu.org/licenses/>.
#

AC_INIT([bgpwatcher], [0.0.0], [corsaro-info@caida.org])

# The following define the version numbers for the libtool-created library
# (libbgpwatcher), and not for the overall package. The package version (above)
# should use semantic versioning, for more info on the library versioning, see
# https://www.sourceware.org/autobook/autobook/autobook_91.html

BGPWATCHER_MAJOR_VERSION=0
BGPWATCHER_MID_VERSION=0
BGPWATCHER_MINOR_VERSION=0

AC_DEFINE_UNQUOTED([BGPWATCHER_MAJOR_VERSION],$BGPWATCHER_MAJOR_VERSION,
	[bgpwatcher major version])
AC_DEFINE_UNQUOTED([BGPWATCHER_MID_VERSION],$BGPWATCHER_MID_VERSION,
	[bgpwatcher mid version])
AC_DEFINE_UNQUOTED([BGPWATCHER_MINOR_VERSION],$BGPWATCHER_MINOR_VERSION,
	[bgpwatcher minor version])


LT_INIT
AM_INIT_AUTOMAKE
AC_CONFIG_MACRO_DIR([m4])
# Checks for programs.
AC_PROG_LIBTOOL
AC_PROG_CXX
AM_PROG_LIBTOOL
AC_PROG_CC_C99
AC_CONFIG_HEADERS([config.h])


AC_CONFIG_SRCDIR([lib/bgpwatcher.c])


AC_SYS_LARGEFILE
AH_VERBATIM([_GNU_SOURCE],
[/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif])


#AC_CHECK_FUNCS([])


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

# Checks for libraries.
# please use libwandio for threaded IO
AC_SEARCH_LIBS([wandio_create], [wandio trace], ,[AC_MSG_ERROR(
		 [libwandio required (included with libtrace since 3.0.15)]
		 )])

AC_CHECK_LIB([czmq], [zctx_new], ,[AC_MSG_ERROR(
		      [CZMQ is required])])

# @@todo add libbgpstream here

## @note AC_CHECK_LIB is for when you know the name of the library,
## AC_SEARCH_LIBS is when you need to determine the library that a specific
## function is in. For example, above we use AC_SEARCH_LIBS to find the
## wandio_create function as it may be in either the libtrace or libwandio
## libraries depending on the version of libtrace in use.

#AC_CHECK_LIB([trace], [trace_get_payload_length], ,[AC_MSG_ERROR(
#		      [libtrace >= 3.0.8 required])])

# Checks for header files.
AC_CHECK_HEADERS([inttypes.h limits.h math.h stdlib.h string.h time.h sys/time.h])

AC_HEADER_ASSERT

AC_SUBST([BGPWATCHER_MAJOR_VERSION])
AC_SUBST([BGPWATCHER_MID_VERSION])
AC_SUBST([BGPWATCHER_MINOR_VERSION])

AC_CONFIG_FILES([Makefile
                bgpcommon/Makefile
		common/Makefile
		common/libpatricia/Makefile
		common/libcsv/Makefile
		bgpcommon/Makefile
		lib/Makefile
		lib/libbgpstore/Makefile
		tools/Makefile
		])
AC_OUTPUT
