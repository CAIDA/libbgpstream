

# AM_CPPFLAGS - The contents of this variable are passed to every compilation that invokes the C preprocessor; it is a list of arguments to the preprocessor. For instance, -I and -D options should be listed here.
AM_CPPFLAGS = 

# Each _LTLIBRARIES variable is a list of libtool libraries to build. 
lib_LTLIBRARIES = libbgpdump.la

# Public header files that need to be installed in order for people to use the library.
include_HEADERS = bgpdump_lib.h bgpdump_attr.h bgpdump_formats.h bgpdump_mstream.h bgpdump_cfile_tools.h bgpdump_util.h bgpdump-config.h

if WITH_WANDIO
IO_SOURCE=bgpdump_cfile_tools_wandio.c
else
IO_SOURCE=bgpdump_cfile_tools.c
endif

libbgpdump_la_SOURCES = bgpdump_attr.h    \
			bgpdump_formats.h \
			bgpdump_lib.h     \
			bgpdump_mstream.h \
			bgpdump_cfile_tools.h     \
			bgpdump_util.h    \
			bgpdump_lib.c     \
			bgpdump_mstream.c \
			$(IO_SOURCE)	  \
			bgpdump_inet_ntop.c       \
			bgpdump_util.c    \
			bgpdump-config.h



libbgpdump_la_CFLAGS = -Wall
libbgpdump_la_CPPFLAGS=
libbgpdump_la_LIBADD = 

libbgpdump_la_LDFLAGS = -version-info @LIBBGPDUMP_MAJOR_VERSION@:@LIBBGPDUMP_MINOR_VERSION@:@LIBBGPDUMP_MID_VERSION@

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
