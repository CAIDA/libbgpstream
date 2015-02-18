#
# This file is part of bgpstream
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

AM_CPPFLAGS = 	-I$(top_srcdir) \
	 	-I$(top_srcdir)/lib \
	 	-I$(top_srcdir)/lib/utils \
	 	-I$(top_srcdir)/common

bin_PROGRAMS =  bgpreader

bgpreader_SOURCES = bgpreader.c
bgpreader_LDADD   = -lbgpstream
bgpreader_LDFLAGS = -L$(top_builddir)/lib


ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
