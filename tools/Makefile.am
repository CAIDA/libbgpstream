#
# bgpstream
#
# Chiara Orsini, CAIDA, UC San Diego
# chiara@caida.org
#
# Copyright (C) 2013 The Regents of the University of California.
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

AM_CPPFLAGS = 	-I$(top_srcdir) \
	 	-I$(top_srcdir)/lib \
	 	-I$(top_srcdir)/common

bin_PROGRAMS =  bgpreader

bgpreader_SOURCES = bgpstream.c
bgpreader_LDADD   = -lbgpstream
bgpreader_LDFLAGS = -L$(top_builddir)/lib


ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
