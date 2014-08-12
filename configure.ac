AC_INIT([libbgpdump], [1.4.99.14], [ris@ripe.net])

LIBBGPDUMP_MAJOR_VERSION=0
LIBBGPDUMP_MID_VERSION=0
LIBBGPDUMP_MINOR_VERSION=0

AC_DEFINE_UNQUOTED([LIBBGPDUMP_MAJOR_VERSION],$LIBBGPDUMP_MAJOR_VERSION,
	[libbgpdump major version])
AC_DEFINE_UNQUOTED([LIBBGPDUMP_MID_VERSION],$LIBBGPDUMP_MID_VERSION,
	[libbgpdump mid version])
AC_DEFINE_UNQUOTED([LIBBGPDUMP_MINOR_VERSION],$LIBBGPDUMP_MINOR_VERSION,
	[libbgpdump minor version])

LT_INIT

AM_INIT_AUTOMAKE

AC_CONFIG_MACRO_DIR([m4])

AC_CONFIG_SRCDIR([lib/bgpdump_lib.c])
AC_CONFIG_HEADERS([lib/bgpdump-config.h])

# Checks for programs.
AC_PROG_LIBTOOL
AC_PROG_CXX
AM_PROG_LIBTOOL
AC_PROG_CC_C99 # C99 is required because of bool
AC_PROG_CC


AC_SYS_LARGEFILE

AH_VERBATIM([_GNU_SOURCE],
[/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif])


# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h inttypes.h limits.h math.h stdlib.h string.h \
			      time.h sys/time.h])
AC_STRUCT_TM

# Check for u_*_t
AC_CHECK_TYPE(u_char_t, , AC_DEFINE(u_char_t, uchar_t, [Define if system headers do not define u_char_t]))
AC_CHECK_TYPE(u_int8_t, , AC_DEFINE(u_int8_t, uint8_t, [Define if system headers do not define u_int8_t]))
AC_CHECK_TYPE(u_int16_t, , AC_DEFINE(u_int16_t, uint16_t, [Define if system headers do not define u_int16_t]))
AC_CHECK_TYPE(u_int32_t, , AC_DEFINE(u_int32_t, uint32_t, [Define if system headers do not define u_int32_t]))


AC_CHECK_LIB(z, gzopen, [], AC_DEFINE(DONT_HAVE_GZ, 1, Define if libz not present))
AC_CHECK_LIB(bz2, BZ2_bzReadOpen, [], AC_DEFINE(DONT_HAVE_BZ2, 1, Define if libbzip2 not present))

# Check for inet_ntoa in -lnsl if not found (Solaris)
AC_CHECK_FUNCS(inet_ntoa, [], AC_CHECK_LIB(nsl, inet_ntoa, [], AC_MSG_ERROR([inet_ntoa not found],1)))
AC_CHECK_FUNCS(inet_ntop, [], AC_CHECK_LIB(nsl, inet_ntop, [], AC_MSG_ERROR([inet_ntop not found],1)))



# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC


AC_SUBST([LIBBGPDUMP_MAJOR_VERSION])
AC_SUBST([LIBBGPDUMP_MID_VERSION])
AC_SUBST([LIBBGPDUMP_MINOR_VERSION])

AC_HEADER_ASSERT

AC_CONFIG_FILES([Makefile
		lib/Makefile
		tools/Makefile
		])
AC_OUTPUT
