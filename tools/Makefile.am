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

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/common -I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/plugins

dist_bin_SCRIPTS =

bin_PROGRAMS = bc2ascii bgpcorsaro

# main bgpcorsaro program
bgpcorsaro_SOURCES = \
	bgpcorsaro.c

bgpcorsaro_LDADD = -lbgpcorsaro

bgpcorsaro_LDFLAGS = -L$(top_builddir)/lib

# bc2ascii program
bc2ascii_SOURCES = \
	bc2ascii.c
bc2ascii_LDADD = -lbgpcorsaro
bc2ascii_LDFLAGS = -L$(top_builddir)/lib

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
