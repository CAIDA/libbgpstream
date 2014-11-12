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

SUBDIRS =

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/common \
	-I$(top_srcdir)/common/libpatricia \
	-I$(top_srcdir)/common/libcsv \
	-I$(top_srcdir)/lib

noinst_LTLIBRARIES = libbgpcorsaroplugins.la

include_HEADERS =
PLUGIN_SRC=
PLUGIN_LIBS=

if WITH_PLUGIN_BGPSTATS
PLUGIN_SRC+=bgpcorsaro_bgpstats.c bgpcorsaro_bgpstats.h
endif
if WITH_PLUGIN_BGPRIBS
SUBDIRS += libbgpribs
PLUGIN_LIBS += $(top_builddir)/lib/plugins/libbgpribs/libbgpribs.la
AM_CPPFLAGS += -I$(top_srcdir)/lib/plugins/libbgpribs
PLUGIN_SRC += bgpcorsaro_bgpribs.c bgpcorsaro_bgpribs.h
endif
if WITH_PLUGIN_FILTERVIS
PLUGIN_SRC+=bgpcorsaro_filtervis.c bgpcorsaro_filtervis.h
endif
if WITH_PLUGIN_DUMP
PLUGIN_SRC+=bgpcorsaro_dump.c bgpcorsaro_dump.h
endif
if WITH_PLUGIN_IPMETA
PLUGIN_SRC+=bgpcorsaro_ipmeta.c bgpcorsaro_ipmeta.h
endif
if WITH_PLUGIN_FILTERGEO
PLUGIN_SRC+=bgpcorsaro_filtergeo.c bgpcorsaro_filtergeo.h
endif
if WITH_PLUGIN_FILTERPFX
PLUGIN_SRC+=bgpcorsaro_filterpfx.c bgpcorsaro_filterpfx.h
endif
if WITH_PLUGIN_TAGSTATS
PLUGIN_SRC+=bgpcorsaro_tagstats.c bgpcorsaro_tagstats.h
endif

################################################################################
# Add new plugins below here using:
# (ordering is not important)
#
# if WITH_PLUGIN_<macro_name>
# PLUGIN_SRC += <full_name>.c <full_name>.h
# endif
#
################################################################################

libbgpcorsaroplugins_la_SOURCES = \
	$(PLUGIN_SRC)

libbgpcorsaroplugins_la_LIBADD = \
	$(PLUGIN_LIBS)

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
