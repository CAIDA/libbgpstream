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

SUBDIRS = 

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/common \
	-I$(top_srcdir)/common/libpatricia \
	-I$(top_srcdir)/common/libanon \
	-I$(top_srcdir)/libcorsaro

noinst_LTLIBRARIES = libcorsaroplugins.la

include_HEADERS = 
PLUGIN_SRC=
PLUGIN_LIBS=

if WITH_PLUGIN_PCAP
PLUGIN_SRC+=corsaro_pcap.c corsaro_pcap.h 
endif
if WITH_PLUGIN_SIXT
PLUGIN_SRC+=corsaro_flowtuple.c corsaro_flowtuple.h 
include_HEADERS+=corsaro_flowtuple.h
endif
if WITH_PLUGIN_DOS
PLUGIN_SRC+=corsaro_dos.c corsaro_dos.h 
include_HEADERS+=corsaro_dos.h
endif
if WITH_PLUGIN_SMEE
PLUGIN_SRC+=corsaro_smee.c corsaro_smee.h
endif
if WITH_PLUGIN_ANON
PLUGIN_SRC+=corsaro_anon.c corsaro_anon.h
endif
if WITH_PLUGIN_GEODB
PLUGIN_SRC+=corsaro_geodb.c corsaro_geodb.h
endif
if WITH_PLUGIN_PFX2AS
PLUGIN_SRC+=corsaro_pfx2as.c corsaro_pfx2as.h 
endif
if WITH_PLUGIN_FILTERGEO
PLUGIN_SRC+=corsaro_filtergeo.c corsaro_filtergeo.h 
endif
if WITH_PLUGIN_FILTERPFX
PLUGIN_SRC+=corsaro_filterpfx.c corsaro_filterpfx.h 
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

libcorsaroplugins_la_SOURCES = \
	$(PLUGIN_SRC)

libcorsaroplugins_la_LIBADD = \
	$(PLUGIN_LIBS)

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
