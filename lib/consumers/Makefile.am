#
# This file is part of bgpwatcher
#
# Copyright (C) 2015 The Regents of the University of California.
# Authors: Alistair King, Chiara Orsini
#
# All rights reserved.
#
# This code has been developed by CAIDA at UC San Diego.
# For more information, contact bgpstream-info@caida.org
#
# This source code is proprietary to the CAIDA group at UC San Diego and may
# not be redistributed, published or disclosed without prior permission from
# CAIDA.
#
# Report any bugs, questions or comments to bgpstream-info@caida.org
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

# Performance Monitor Consumer
CONSUMER_SRCS += \
	bwc_perfmonitor.c \
	bwc_perfmonitor.h

# Visibility Consumer
CONSUMER_SRCS += \
	bwc_visibility.c \
	bwc_visibility.h

# Per-AS Visibility Consumer (depends on Visibility)
CONSUMER_SRCS += \
	bwc_perasvisibility.c \
	bwc_perasvisibility.h

# Per-Geo Visibility Consumer (depends on Visibility)
CONSUMER_SRCS += \
	bwc_pergeovisibility.c \
	bwc_pergeovisibility.h

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
