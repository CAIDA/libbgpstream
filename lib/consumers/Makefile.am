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

SUBDIRS =

AM_CPPFLAGS = 	-I$(top_srcdir)			\
		-I$(top_srcdir)/bgpcommon 	\
		-I$(top_srcdir)/common 		\
		-I$(top_srcdir)/lib

noinst_LTLIBRARIES = libbgpwatcher_consumers.la

include_HEADERS =

CONSUMER_SRCS=
CONSUMER_LIBS=

# Test Consumer
CONSUMER_SRCS += \
	bwc_test.c \
	bwc_test.h

# Per-AS Visibility Consumer
#CONSUMER_SRCS += \
#	bwc_perasvisibility.c \
#	bwc_perasvisibility.h

# -- sample how to add conditional consumer
#if WITH_<NAME>
#SUBDIRS += lib<name>
#AM_CPPFLAGS += -I$(top_srcdir)/lib/consumers/lib<name>
#CONSUMER_SRCS += \
#	bwc_<name>.c \
#	bwc_<name>.h
#CONSUMER_LIBS += $(top_builddir)/lib/consumers/lib<name>/lib<name>.la
#endif

libbgpwatcher_consumers_la_SOURCES = $(CONSUMER_SRCS)

libbgpwatcher_consumers_la_LIBADD = $(CONSUMER_LIBS)

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
