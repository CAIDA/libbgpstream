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

SUBDIRS = common docs libcorsaro tools
AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/libcorsaro -I$(top_srcdir)/common \
	-I$(top_srcdir)/libcorsaro/plugins

bin_PROGRAMS = corsaro

# main corsaro program
corsaro_SOURCES = \
	corsaro_main.c

corsaro_LDADD = -lcorsaro

corsaro_LDFLAGS = -L$(top_builddir)/libcorsaro

EXTRA_DIST = libtrace-3.0.13-wandio_export.patch

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
