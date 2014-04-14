#
# corsaro
# 
# Alistair King, CAIDA, UC San Diego
# corsaro-info@caida.org
# 
# Copyright (C) 2012 The Regents of the University of California.
# 
# This file is part of corsaro.
# 
# corsaro is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# corsaro is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with corsaro.  If not, see <http://www.gnu.org/licenses/>.
# 

AC_PREREQ([2.68])

AC_INIT([corsaro], [2.0.0], [corsaro-info@caida.org])

# Corsaro versions...
# this may need some re-thinking
# currently increment:
# MAJOR: if the interfaces have been added, removed or changed
# MID:   if source code has been changed
# MINOR: if interfaces have been added
#
# there will need to be some thought put into the binary file versioning
# if the format changes
# perhaps use the major version to indicate changes in file format

CORSARO_MAJOR_VERSION=2
CORSARO_MID_VERSION=0
CORSARO_MINOR_VERSION=0

AC_DEFINE_UNQUOTED([CORSARO_MAJOR_VERSION],$CORSARO_MAJOR_VERSION,
	[corsaro major version])
AC_DEFINE_UNQUOTED([CORSARO_MID_VERSION],$CORSARO_MID_VERSION,
	[corsaro mid version])
AC_DEFINE_UNQUOTED([CORSARO_MINOR_VERSION],$CORSARO_MINOR_VERSION,
	[corsaro minor version])

LT_INIT

AM_INIT_AUTOMAKE

AC_CONFIG_MACRO_DIR([m4])

AC_CONFIG_SRCDIR([libcorsaro/corsaro.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_LIBTOOL
AC_PROG_CC_C99

AC_SYS_LARGEFILE

AH_VERBATIM([_GNU_SOURCE],
[/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif])

AC_CHECK_PROG([corsaro_doxygen_avail], [doxygen], [yes], [no])

AC_MSG_CHECKING([whether to compile the docs])
AC_ARG_WITH([doxygen],
	[AS_HELP_STRING([--without-doxygen],
	  [do not compile the doxygen generated docs])],
	  [corsaro_doxygen=no],
	  [if test "x$corsaro_doxygen_avail" == xyes; then
	      corsaro_doxygen=yes
	  else
		corsaro_doxygen=no
	  fi
	  ])
AC_MSG_RESULT([$corsaro_doxygen])

AM_CONDITIONAL(HAS_DOXYGEN, [test x"$corsaro_doxygen" = xyes])

AC_CHECK_FUNCS([gettimeofday memset strdup strstr strsep strlcpy vasprintf])

# OSX Mavericks has replaced strlcpy with a macro that is not found by
# AC_CHECK_FUNCS
AC_CHECK_DECLS([strlcpy])

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

# Configure options for man pages
AC_ARG_WITH(man,
	    AS_HELP_STRING(--with-man,install man pages by default),[
	     if test "$withval" = yes
	     then
		     MANPAGES="docs/man/man3/[a-zA-Z]*.3"
	     else
		     MANPAGES=""
	     fi
])

# Checks for typedefs, structures, and compiler characteristics.
#AC_C_INLINE # 2014-04-14 AK removes because it causes problems with clang3.4
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC


AX_LIB_SOCKET_NSL


# Checks for libraries.
AC_CHECK_LIB([packetdump], [trace_dump_packet], ,[AC_MSG_WARN(
			   [libpacketdump missing. ascii packet dumps not available])])
AC_CHECK_LIB([trace], [trace_get_payload_length], ,[AC_MSG_ERROR(
		      [libtrace >= 3.0.8 required])])
AC_SEARCH_LIBS([wandio_create], [wandio trace], ,[AC_MSG_ERROR(
		 [libwandio required (included with libtrace since 3.0.15)]
		 )])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h inttypes.h limits.h math.h stdlib.h string.h \
			      time.h sys/time.h])

# configure options for this monitor

# monitor name
AC_MSG_CHECKING([for the monitor name to use])
AC_ARG_WITH([monitorname],
	[AS_HELP_STRING([--with-monitorname=NAME],
	  [explicitly define the monitor name (defaults to system hostname)])],
	  [if test "x$withval" == xyes || test "x$withval" == ""; then
	  with_monitorname=`hostname`
	  else
	  with_monitorname=$withval
	  fi],
	  [with_monitorname=`hostname`])
AC_MSG_RESULT([$with_monitorname])

AC_DEFINE_UNQUOTED([CORSARO_MONITOR_NAME],[$with_monitorname],[Monitor Name])

# are we dealing with a slash eight?
AC_MSG_CHECKING([whether to use /8 optimizations])
AC_ARG_WITH([slash-eight],
	[AS_HELP_STRING([--with-slash-eight=FIRST_OCTET],
	  [make use of optimizations for a /8 darknet (defaults to no)])],
	  [if test "x$withval" == xyes || test "x$withval" == ""; then
	      AC_MSG_ERROR([--with-slash-eight requires an octet value])
	   else
	      with_slash_eight=$withval
	   fi],
	   [with_slash_eight=no])

AS_IF([test "x$with_slash_eight" != xno],
      	[
	    AC_DEFINE_UNQUOTED([CORSARO_SLASH_EIGHT],[$with_slash_eight],
		[Slash Eight Octet])
    	])

AC_MSG_RESULT([$with_slash_eight])

# configure options to disable plugins
#AC_DEFINE([ED_PLUGIN_INIT_ALL_ENABLED],[],[Plugin init functions])

################################################################################
# Plugin Configuration Macros                                                  #
# ---------------------------                                                  #
#                                                                              #
# Each ED_WITH_PLUGIN macro defines a plugin which can be compiled into        #
# Corsaro.                                                                     #
#                                                                              #
# The order in which these plugins are listed is the _default_ order in which  #
# they will be run.                                                            #
################################################################################

## meta plugins. these do not write output, they add metadata to the packet
## seems that the most common use-case will be to run them first
ED_WITH_PLUGIN([corsaro_anon],[anon],[ANON],[no])
ED_WITH_PLUGIN([corsaro_ipmeta],[ipmeta],[IPMETA],[no])

# filter plugins. these do not write output, they indicate which packets 
# should [not] be written out
ED_WITH_PLUGIN([corsaro_filtergeo],[filtergeo],[FILTERGEO],[no])
ED_WITH_PLUGIN([corsaro_filterpfx],[filterpfx],[FILTERPFX],[no])
ED_WITH_PLUGIN([corsaro_filterbpf],[filterbpf],[FILTERBPF],[no])

#pass-through pcap plugin
# the pcap plugin only makes sense in live mode, so disable it by default
ED_WITH_PLUGIN([corsaro_pcap],[pcap],[PCAP],[no])

# default plugins
ED_WITH_PLUGIN([corsaro_flowtuple],[flowtuple],[SIXT],[yes])

# optional plugins
ED_WITH_PLUGIN([corsaro_dos],[dos],[DOS],[no])
ED_WITH_PLUGIN([corsaro_smee],[smee],[SMEE],[no])

# this MUST go after all the ED_WITH_PLUGIN macro calls
AC_DEFINE_UNQUOTED([ED_PLUGIN_INIT_ALL_ENABLED], $ED_PLUGIN_INIT_ALL_ENABLED,
		   [plugins to call the init macro for in corsaro_plugin.c])

#------------------------------------------------------------------------------#
# Extra tests or config needed for plugins should go below here                #
#------------------------------------------------------------------------------#

# the ipmeta plugin requires libipmeta
if test "x$with_plugin_ipmeta" == xyes; then
   AC_CHECK_LIB([ipmeta], [ipmeta_init], ,[
   	AC_MSG_ERROR([libipmeta is required by the corsaro_ipmeta plugin])
		      ])
fi

# the filtergeo plugin requires the ipmeta plugin
if test "x$with_plugin_filtergeo" == xyes &&
   test "x$with_plugin_ipmeta" != xyes; then
   AC_MSG_ERROR([corsaro_filtergeo plugin requires corsaro_ipmeta plugin])
fi

# the dos plugin needs flowtuple also
if test "x$with_plugin_dos" == xyes && 
   test "x$with_plugin_flowtuple" != xyes; then
   AC_MSG_ERROR([corsaro_dos plugin requires corsaro_flowtuple plugin])
fi

# these are only used by libsmee
if test "x$with_plugin_smee" == xyes; then
   AC_CHECK_LIB([smee], [iat_init], ,[
   			AC_MSG_ERROR([libsmee >= 2.2.2 is required (see http://www.caida.org/tools/measurement/corsaro/docs/plugins.html@%:@plugins_smee)])
		      ])
fi

# unfortunately we have to generate all of these Makefiles because of make dist
# they will not however be compiled into the binary unless they are used

################################################################################
# END PLUGIN CONFIGURATION                                                     #
################################################################################

AC_HEADER_ASSERT

AC_SUBST([CORSARO_MAJOR_VERSION])
AC_SUBST([CORSARO_MID_VERSION])
AC_SUBST([CORSARO_MINOR_VERSION])

AC_HEADER_ASSERT

AC_CONFIG_FILES([Makefile libcorsaro/Makefile libcorsaro/plugins/Makefile
			  libcorsaro/plugins/libanon/Makefile
			  tools/Makefile common/Makefile
			  common/libpatricia/Makefile
			  common/libcsv/Makefile
			  docs/Makefile
			  docs/corsaro.doxygen
			  docs/corsaro.caida.doxygen])
AC_OUTPUT
