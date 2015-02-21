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

AM_CPPFLAGS = -I$(top_srcdir) \
	      -I$(top_srcdir)/common \
	      -I$(top_srcdir)/lib

dist_bin_SCRIPTS =

bin_PROGRAMS =	bgpwatcher-server \
		bgpwatcher-consumer \
		bgpwatcher-test-producer

# runs the bgpwatcher server
bgpwatcher_server_SOURCES = \
	bgpwatcher-server.c
bgpwatcher_server_LDADD = -lbgpwatcher
bgpwatcher_server_LDFLAGS = -L$(top_builddir)/lib


# connects to the bgpwatcher server and drives consumer plugins
bgpwatcher_consumer_SOURCES = \
	bgpwatcher-consumer.c
bgpwatcher_consumer_LDADD = -lbgpwatcher
bgpwatcher_consumer_LDFLAGS = -L$(top_builddir)/lib

# acts as a dummy producer for the bgpwatcher server
bgpwatcher_test_producer_SOURCES = \
	bgpwatcher-test-producer.c
bgpwatcher_test_producer_LDADD = -lbgpwatcher
bgpwatcher_test_producer_LDFLAGS = -L$(top_builddir)/lib

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
