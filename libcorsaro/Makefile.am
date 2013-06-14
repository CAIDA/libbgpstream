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

SUBDIRS = plugins
AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/common \
	-I$(top_srcdir)/common/libpatricia \
	-I$(top_srcdir)/libcorsaro/plugins

lib_LTLIBRARIES = libcorsaro.la

include_HEADERS = corsaro.h corsaro_log.h corsaro_io.h

libcorsaro_la_SOURCES = 	\
	corsaro.c 		\
	corsaro.h 		\
	corsaro_file.c 		\
	corsaro_file.h 		\
	corsaro_geo.c 		\
	corsaro_geo.h 		\
	corsaro_int.h 		\
	corsaro_io.c 		\
	corsaro_io.h 		\
	corsaro_log.c 		\
	corsaro_log.h 		\
	corsaro_plugin.c 	\
	corsaro_plugin.h 

libcorsaro_la_LIBADD = $(top_builddir)/common/libcorsarocommon.la \
	$(top_builddir)/libcorsaro/plugins/libcorsaroplugins.la

libcorsaro_la_LDFLAGS = -version-info @CORSARO_MAJOR_VERSION@:@CORSARO_MINOR_VERSION@:@CORSARO_MID_VERSION@

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
