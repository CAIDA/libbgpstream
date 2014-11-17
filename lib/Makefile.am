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


# AM_CPPFLAGS - The contents of this variable are passed to every compilation that invokes the C preprocessor; it is a list of arguments to the preprocessor. For instance, -I and -D options should be listed here.
AM_CPPFLAGS = 	-I$(top_srcdir) \
	 	-I$(top_srcdir)/common \
                -I$(top_srcdir)/bgpcommon

# Each _LTLIBRARIES variable is a list of libtool libraries to build. 
lib_LTLIBRARIES = libbgpstream.la

# Public header files that need to be installed in order for people to use the library.
include_HEADERS = bgpstream_lib.h   \
		  bgpstream_record.h \
		  bgpstream_options.h


libbgpstream_la_SOURCES = 	\
	bgpstream_lib.h		\
	bgpstream_int.h		\
	bgpstream_lib.c		\
	bgpstream_options.h	\
	bgpstream_elem.h	\
	bgpstream_record.h	\
	bgpstream_constants.h	\
	bgpstream_reader.h	\
	bgpstream_reader.c	\
	bgpstream_input.h	\
	bgpstream_input.c	\
	bgpstream_filter.h	\
	bgpstream_filter.c	\
	bgpstream_datasource.h	\
	bgpstream_datasource.c	\
	bgpstream_elem.c	\
	bgpstream_debug.h



libbgpstream_la_CFLAGS = -Wall

libbgpstream_la_LIBADD = $(top_builddir)/common/libcccommon.la \
			 $(top_builddir)/bgpcommon/libbl.la	

libbgpstream_la_LDFLAGS = -version-info @BGPSTREAM_MAJOR_VERSION@:@BGPSTREAM_MINOR_VERSION@:@BGPSTREAM_MID_VERSION@

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
