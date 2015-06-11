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

SUBDIRS =

AM_CPPFLAGS = -I$(top_srcdir)             \
	      -I$(top_srcdir)/lib/bgpdump \
	      -I$(top_srcdir)/common


noinst_LTLIBRARIES = libbgpstream-utils.la

CONDITIONAL_LIBS=

include_HEADERS= bgpstream_utils.h 		     \
		 bgpstream_utils_addr.h 	     \
		 bgpstream_utils_addr_set.h	     \
		 bgpstream_utils_as_path.h	     \
		 bgpstream_utils_id_set.h     	     \
		 bgpstream_utils_peer_sig_map.h      \
		 bgpstream_utils_pfx.h		     \
		 bgpstream_utils_pfx_set.h	     \
		 bgpstream_utils_str_set.h

libbgpstream_utils_la_SOURCES =             \
	bgpstream_utils.h                   \
	bgpstream_utils_addr.c              \
	bgpstream_utils_addr.h              \
	bgpstream_utils_addr_set.c 	    \
	bgpstream_utils_addr_set.h	    \
	bgpstream_utils_as_path.c	    \
	bgpstream_utils_as_path.h	    \
	bgpstream_utils_as_path_int.h	    \
	bgpstream_utils_id_set.c     	    \
	bgpstream_utils_id_set.h     	    \
	bgpstream_utils_peer_sig_map.c      \
	bgpstream_utils_peer_sig_map.h      \
	bgpstream_utils_pfx.c		    \
	bgpstream_utils_pfx.h		    \
	bgpstream_utils_pfx_set.c  	    \
	bgpstream_utils_pfx_set.h	    \
	bgpstream_utils_str_set.c  	    \
	bgpstream_utils_str_set.h


libbgpstream_utils_la_LIBADD = $(CONDITIONAL_LIBS)

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
