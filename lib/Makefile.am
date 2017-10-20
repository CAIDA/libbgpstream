#
# This file is part of bgpstream
#
# CAIDA, UC San Diego
# bgpstream-info@caida.org
#
# Copyright (C) 2012 The Regents of the University of California.
# Authors: Alistair King, Chiara Orsini
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 2 of the License, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program.  If not, see <http://www.gnu.org/licenses/>.
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
