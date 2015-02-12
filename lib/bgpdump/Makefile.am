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

SUBDIRS=

# to allow #include <config.h>
AM_CPPFLAGS= -I$(top_srcdir)

noinst_LTLIBRARIES = libbgpstream-bgpdump.la

if WITH_WANDIO
IO_SOURCE=bgpdump_cfile_tools_wandio.c
else
IO_SOURCE=bgpdump_cfile_tools.c
endif

libbgpstream_bgpdump_la_SOURCES = 	\
	bgpdump_attr.h    		\
	bgpdump_cfile_tools.h     	\
	$(IO_SOURCE)	  		\
	bgpdump_formats.h 		\
	bgpdump_lib.c     		\
	bgpdump_lib.h     		\
	bgpdump_mstream.c 		\
	bgpdump_mstream.h 		\
	bgpdump_process.c		\
	bgpdump_process.h		\
	bgpdump_util.c			\
	bgpdump_util.h

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
