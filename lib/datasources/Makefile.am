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

SUBDIRS=

# to allow #include <config.h>
AM_CPPFLAGS= 	-I$(top_srcdir) \
		-I$(top_srcdir)/lib \
                -I$(top_srcdir)/lib/utils \
	 	-I$(top_srcdir)/common


noinst_LTLIBRARIES = libbgpstream-datasources.la

libbgpstream_datasources_la_SOURCES = 	\
	bgpstream_datasource_mysql.h	\
	bgpstream_datasource_mysql.c	\
	bgpstream_datasource_singlefile.h	\
	bgpstream_datasource_singlefile.c	\
	bgpstream_datasource_csvfile.h		\
	bgpstream_datasource_csvfile.c		\
	bgpstream_datasource_sqlite.h		\
	bgpstream_datasource_sqlite.c		\
	bgpstream_datasource_broker.h		\
	bgpstream_datasource_broker.c

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
