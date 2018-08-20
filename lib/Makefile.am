#
# Copyright (C) 2014 The Regents of the University of California.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

SUBDIRS= datainterfaces formats transports utils

# AM_CPPFLAGS - The contents of this variable are passed to every compilation
# that invokes the C preprocessor; it is a list of arguments to the
# preprocessor. For instance, -I and -D options should be listed here.
AM_CPPFLAGS = 	-I$(top_srcdir) \
		-I$(top_srcdir)/lib/datainterfaces \
		-I$(top_srcdir)/lib/formats \
		-I$(top_srcdir)/lib/transports \
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
	bgpstream_bgpdump.c	\
	bgpstream_bgpdump.h	\
	bgpstream_constants.h	\
	bgpstream_di_interface.h	\
	bgpstream_di_mgr.c	\
	bgpstream_di_mgr.h	\
	bgpstream_elem.c	\
	bgpstream_elem.h	\
	bgpstream_elem_int.h	\
	bgpstream_elem_generator.c \
	bgpstream_elem_generator.h \
	bgpstream_filter.h	\
	bgpstream_filter.c	\
	bgpstream_filter_parser.h	\
	bgpstream_filter_parser.c	\
	bgpstream_format.h	\
	bgpstream_format.c	\
	bgpstream_format_interface.h	\
	bgpstream_int.h		\
	bgpstream_log.c		\
	bgpstream_log.h		\
	bgpstream_reader.c	\
	bgpstream_reader.h	\
	bgpstream_record.c	\
	bgpstream_record.h	\
	bgpstream_record_int.h	\
	bgpstream_resource.c	\
	bgpstream_resource.h	\
	bgpstream_resource_mgr.c	\
	bgpstream_resource_mgr.h	\
	bgpstream_transport.h	\
	bgpstream_transport.c	\
	bgpstream_transport_interface.h


libbgpstream_la_CFLAGS = -Wall

libbgpstream_la_LIBADD = \
			$(top_builddir)/lib/datainterfaces/libbgpstream-data-interfaces.la \
			$(top_builddir)/lib/transports/libbgpstream-transports.la \
			$(top_builddir)/lib/formats/libbgpstream-formats.la \
			$(top_builddir)/lib/utils/libbgpstream-utils.la \
			$(top_builddir)/common/libcccommon.la

libbgpstream_la_LDFLAGS = -version-info @LIBBGPSTREAM_MAJOR_VERSION@:@LIBBGPSTREAM_MINOR_VERSION@:@LIBBGPSTREAM_MID_VERSION@

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
