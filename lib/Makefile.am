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

SUBDIRS= bgpdump datasources utils

# AM_CPPFLAGS - The contents of this variable are passed to every compilation
# that invokes the C preprocessor; it is a list of arguments to the
# preprocessor. For instance, -I and -D options should be listed here.
AM_CPPFLAGS = 	-I$(top_srcdir) \
		-I$(top_srcdir)/lib/bgpdump \
		-I$(top_srcdir)/lib/datasources \
                -I$(top_srcdir)/lib/utils \
	 	-I$(top_srcdir)/common

# Each _LTLIBRARIES variable is a list of libtool libraries to build.
lib_LTLIBRARIES = libbgpstream.la

# Public header files that need to be installed in order for people to use the
# library.
include_HEADERS = bgpstream.h		\
		  bgpstream_elem.h	\
		  bgpstream_record.h


libbgpstream_la_SOURCES = 	\
	bgpstream.h		\
	bgpstream.c		\
	bgpstream_constants.h	\
	bgpstream_datasource.c	\
	bgpstream_datasource.h	\
	bgpstream_debug.h	\
	bgpstream_elem.c	\
	bgpstream_elem.h	\
	bgpstream_elem_int.h	\
	bgpstream_elem_generator.c \
	bgpstream_elem_generator.h \
	bgpstream_filter.h	\
	bgpstream_filter.c	\
	bgpstream_input.h	\
	bgpstream_input.c	\
	bgpstream_int.h		\
	bgpstream_reader.c	\
	bgpstream_reader.h	\
	bgpstream_record.c	\
	bgpstream_record.h


libbgpstream_la_CFLAGS = -Wall

libbgpstream_la_LIBADD = $(top_builddir)/lib/bgpdump/libbgpstream-bgpdump.la \
			 $(top_builddir)/lib/datasources/libbgpstream-datasources.la \
			 $(top_builddir)/lib/utils/libbgpstream-utils.la \
			 $(top_builddir)/common/libcccommon.la

libbgpstream_la_LDFLAGS = -version-info @BGPSTREAM_MAJOR_VERSION@:@BGPSTREAM_MINOR_VERSION@:@BGPSTREAM_MID_VERSION@

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
