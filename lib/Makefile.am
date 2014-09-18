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

SUBDIRS =
AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/common

lib_LTLIBRARIES = libbgpwatcher.la

include_HEADERS = 			\
	bgpwatcher.h 			\
	bgpwatcher_client.h 		\
	bgpwatcher_common.h

libbgpwatcher_la_SOURCES = 		\
	bgpwatcher.c 			\
	bgpwatcher.h			\
	bgpwatcher_int.h		\
	bgpwatcher_common.h		\
	bgpwatcher_common.c		\
	bgpwatcher_client.h		\
	bgpwatcher_client_int.h		\
	bgpwatcher_client.c		\
	bgpwatcher_server.h		\
	bgpwatcher_server.c

libbgpwatcher_la_LIBADD = $(top_builddir)/common/libcccommon.la

libbgpwatcher_la_LDFLAGS = -version-info @BGPWATCHER_MAJOR_VERSION@:@BGPWATCHER_MID_VERSION@:@BGPWATCHER_MINOR_VERSION@

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
