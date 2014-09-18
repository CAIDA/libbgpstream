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

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/common -I$(top_srcdir)/lib

dist_bin_SCRIPTS =

bin_PROGRAMS = bgpwatcher-server

bgpwatcher_server_SOURCES = \
	bgpwatcher-server.c
bgpwatcher_server_LDADD = -lbgpwatcher
bgpwatcher_server_LDFLAGS = -L$(top_builddir)/lib

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
