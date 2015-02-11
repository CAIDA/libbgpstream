#
# bgpstream
#
# Chiara Orsini, CAIDA, UC San Diego
# chiara@caida.org
#
# Copyright (C) 2014 The Regents of the University of California.
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

SUBDIRS =

AM_CPPFLAGS = -I$(top_srcdir)  \
	      -I$(top_srcdir)/common


noinst_LTLIBRARIES = libbgpstream-utils.la

CONDITIONAL_LIBS=

include_HEADERS= bgpstream_utils.h \
		bgpstream_utils_addr.h

libbgpstream_utils_la_SOURCES =             \
	bgpstream_utils.h                   \
	bgpstream_utils_addr.c              \
	bgpstream_utils_addr.h              \
	bgpstream_utils_as.c		    \
	bgpstream_utils_as.h		    \
	bgpstream_utils_peer_sig_map.c      \
	bgpstream_utils_peer_sig_map_int.h  \
	bgpstream_utils_peer_sig_map.h      \
	bgpstream_utils_pfx.c		    \
	bgpstream_utils_pfx.h		    \
	\
	bgpstream_utils_str_set.c  \
	bgpstream_utils_str_set.h  \
	bgpstream_utils_id_set.c     \
	bgpstream_utils_id_set.h     \
	bgpstream_utils_pfx_set.c  \
	bgpstream_utils_pfx_set_int.h  \
	bgpstream_utils_pfx_set.h  \
	bgpstream_utils_addr_set.c \
	bgpstream_utils_addr_set.h


libbgpstream_utils_la_LIBADD = $(CONDITIONAL_LIBS)

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
