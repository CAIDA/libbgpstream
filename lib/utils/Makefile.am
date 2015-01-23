#
# bgp-common
#
# Chiara Orsini, CAIDA, UC San Diego
# chiara@caida.org
#
# Copyright (C) 2014 The Regents of the University of California.
#
# This file is part of bgp-common.
#
# bgp-common is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# bgp-common is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with bgp-common.  If not, see <http://www.gnu.org/licenses/>.
#

SUBDIRS =

AM_CPPFLAGS = -I$(top_srcdir)  \
	      -I$(top_srcdir)/common


noinst_LTLIBRARIES = libbgpstream-utils.la

CONDITIONAL_LIBS=

include_HEADERS= bl_bgp_utils.h

libbgpstream_utils_la_SOURCES =   \
	bl_bgp_utils.c  \
	bl_bgp_utils.h  \
	bl_peersign_map.c  \
	bl_peersign_map_int.h  \
	bl_peersign_map.h  \
	bl_str_set.c  \
	bl_str_set.h  \
	bl_id_set.c     \
	bl_id_set.h     \
	bl_as_set.c     \
	bl_as_set.h     \
	bl_pfx_set.c  \
	bl_pfx_set_int.h  \
	bl_pfx_set.h  \
	bl_addr_set.c \
	bl_addr_set.h


libbgpstream_utils_la_LIBADD = $(CONDITIONAL_LIBS)

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
