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

SUBDIRS = libpatricia libcsv

AM_CPPFLAGS = -I$(top_srcdir) \
	-I$(top_srcdir)/common/libpatricia \
	-I$(top_srcdir)/common/libcsv

noinst_LTLIBRARIES = libcorsarocommon.la

CONDITIONAL_LIBS=

if WITH_LIBANON
SUBDIRS += libanon
CONDITIONAL_LIBS += $(top_builddir)/common/libanon/libanon.la
AM_CPPFLAGS += -I$(top_srcdir)/common/libanon
endif

libcorsarocommon_la_SOURCES = \
	utils.c \
	utils.h \
	ip_utils.c \
	ip_utils.h \
	parse_cmd.c \
	parse_cmd.h \
	khash.h \
	ksort.h

libcorsarocommon_la_LIBADD = $(top_builddir)/common/libpatricia/libpatricia.la \
			$(top_builddir)/common/libcsv/libcsv.la \
			$(CONDITIONAL_LIBS)

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
