

#
# libbgpstream
#
# Chiara Orsini, CAIDA, UC San Diego
# chiara@caida.org
#
# Copyright (C) 2013 The Regents of the University of California.
#
# This file is part of libbgpstream.
#
# libbgpstream is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# libbgpstream is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with libbgpstream.  If not, see <http://www.gnu.org/licenses/>.
#

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/lib 


bin_PROGRAMS =  bgpstream-test-customlist bgpstream-test-csvfile bgpstream-best-collectors

bgpstream_test_customlist_SOURCES = bgpstream-test-csvfile.c process_bdump.c print_elem_queue.c
bgpstream_test_customlist_LDADD = -lbgpstream 
bgpstream_test_customlist_LDFLAGS = -L$(top_builddir)/lib

bgpstream_test_csvfile_SOURCES = bgpstream-test-csvfile.c process_bdump.c print_elem_queue.c
bgpstream_test_csvfile_LDADD = -lbgpstream 
bgpstream_test_csvfile_LDFLAGS = -L$(top_builddir)/lib


bgpstream_best_collectors_SOURCES = bgpstream-best-collectors.c process_bdump.c
bgpstream_best_collectors_LDADD = -lbgpstream 
bgpstream_best_collectors_LDFLAGS = -L$(top_builddir)/lib


ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
