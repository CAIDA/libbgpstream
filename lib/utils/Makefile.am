#
# Copyright (C) 2014 The Regents of the University of California.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

SUBDIRS =

AM_CPPFLAGS = -I$(top_srcdir)             \
	      -I$(top_srcdir)/lib	  \
	      -I$(top_srcdir)/common


noinst_LTLIBRARIES = libbgpstream-utils.la

CONDITIONAL_LIBS=

include_HEADERS= bgpstream_utils.h 		     \
		 bgpstream_utils_addr.h 	     \
		 bgpstream_utils_addr_set.h	     \
		 bgpstream_utils_as_path.h	     \
		 bgpstream_utils_as_path_store.h     \
		 bgpstream_utils_community.h	     \
		 bgpstream_utils_id_set.h     	     \
		 bgpstream_utils_peer_sig_map.h      \
		 bgpstream_utils_pfx.h		     \
		 bgpstream_utils_pfx_set.h	     \
		 bgpstream_utils_str_set.h	     \
		 bgpstream_utils_ip_counter.h	     \
	         bgpstream_utils_patricia.h  \
		 bgpstream_utils_time.h


libbgpstream_utils_la_SOURCES =             \
	bgpstream_utils.h                   \
	bgpstream_utils_addr.c              \
	bgpstream_utils_addr.h              \
	bgpstream_utils_addr_set.c 	    \
	bgpstream_utils_addr_set.h	    \
	bgpstream_utils_as_path.c	    \
	bgpstream_utils_as_path.h	    \
	bgpstream_utils_as_path_store.c	    \
	bgpstream_utils_as_path_store.h	    \
	bgpstream_utils_as_path_int.h	    \
	bgpstream_utils_community.h	    \
	bgpstream_utils_community.c	    \
	bgpstream_utils_community_int.h	    \
	bgpstream_utils_id_set.c     	    \
	bgpstream_utils_id_set.h     	    \
	bgpstream_utils_peer_sig_map.c      \
	bgpstream_utils_peer_sig_map.h      \
	bgpstream_utils_pfx.c		    \
	bgpstream_utils_pfx.h		    \
	bgpstream_utils_pfx_set.c  	    \
	bgpstream_utils_pfx_set.h	    \
	bgpstream_utils_str_set.c  	    \
	bgpstream_utils_str_set.h	    \
	bgpstream_utils_ip_counter.c	    \
	bgpstream_utils_ip_counter.h	    \
	bgpstream_utils_patricia.c	    \
	bgpstream_utils_patricia.h		\
	bgpstream_utils_time.c    \
	bgpstream_utils_time.h


libbgpstream_utils_la_LIBADD = $(CONDITIONAL_LIBS)

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
