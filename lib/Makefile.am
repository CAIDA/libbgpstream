#
# bgpcorsaro
#
# Alistair King, CAIDA, UC San Diego
# corsaro-info@caida.org
#
# Copyright (C) 2012 The Regents of the University of California.
#
# This file is part of bgpcorsaro.
#
# bgpcorsaro is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# bgpcorsaro is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with bgpcorsaro.  If not, see <http://www.gnu.org/licenses/>.
#

SUBDIRS = plugins
AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/common \
	-I$(top_srcdir)/common/libpatricia \
	-I$(top_srcdir)/lib/plugins

lib_LTLIBRARIES = libbgpcorsaro.la

include_HEADERS = bgpcorsaro.h bgpcorsaro_log.h bgpcorsaro_io.h

libbgpcorsaro_la_SOURCES = 	\
	bgpcorsaro.c 		\
	bgpcorsaro.h 		\
	bgpcorsaro_int.h 	\
	bgpcorsaro_io.c 	\
	bgpcorsaro_io.h 	\
	bgpcorsaro_log.c	\
	bgpcorsaro_log.h	\
	bgpcorsaro_plugin.c 	\
	bgpcorsaro_plugin.h	\
	bgpcorsaro_tag.c	\
	bgpcorsaro_tag.h

libbgpcorsaro_la_LIBADD = $(top_builddir)/common/libcccommon.la \
		          $(top_builddir)/lib/plugins/libbgpcorsaroplugins.la

libbgpcorsaro_la_LDFLAGS = -version-info @BGPCORSARO_MAJOR_VERSION@:@BGPCORSARO_MINOR_VERSION@:@BGPCORSARO_MID_VERSION@

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
