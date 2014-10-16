#
# bgpcorsaro
#
# Chiara Orsini, CAIDA, UC San Diego
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

AM_CPPFLAGS = -I$(top_srcdir)/common 

noinst_LTLIBRARIES = libbgpribs.la

libbgpribs_la_SOURCES = bgpribs_lib.h   \
			bgpribs_int.h   \
			bgpribs_lib.c   \
			bgpribs_collectors_table.h	\
			bgpribs_collectors_table.c	\
			bgpribs_collectordata.h	\
			bgpribs_collectordata.c	\
			bgpribs_peers_table.h	\
			bgpribs_peers_table.c	\
			bgpribs_peerdata.h	\
			bgpribs_peerdata.c	\
			bgpribs_ribs_table.h	\
			bgpribs_ribs_table.c	\
			bgpribs_prefixes_table.h	\
			bgpribs_prefixes_table.c	\
			bgpribs_ases_table.h	\
			bgpribs_ases_table.c	\
			bgpribs_bgpwatcher_client.h	\
			bgpribs_bgpwatcher_client.c	\
			bgpribs_khash.h	\
			bgpribs_khash.c	\
			bgpribs_common.h	\
			bgpribs_common.c	

# libbgpribs_la_LIBADD =


ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~

