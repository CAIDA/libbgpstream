#
# Copyright (C) 2017 The Regents of the University of California.
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

SUBDIRS= libparsebgp

# to allow #include <config.h>
AM_CPPFLAGS= 	-I$(top_srcdir) \
		-I$(top_srcdir)/lib \
                -I$(top_srcdir)/lib/utils \
	 	-I$(top_srcdir)/common \
		-I$(top_srcdir)/lib/formats/libparsebgp/lib \
		-I$(top_srcdir)/lib/formats/libparsebgp/lib/bgp \
		-I$(top_srcdir)/lib/formats/libparsebgp/lib/mrt \
		-I$(top_srcdir)/lib/formats/libparsebgp/lib/bmp


noinst_LTLIBRARIES = libbgpstream-formats.la

SOURCES= 				\
	bs_format_bmp.c			\
	bs_format_bmp.h			\
	bs_format_mrt.c 		\
	bs_format_mrt.h 		\
	bgpstream_parsebgp_common.c	\
	bgpstream_parsebgp_common.h

LIBS=$(top_builddir)/lib/formats/libparsebgp/lib/libparsebgp.la

libbgpstream_formats_la_SOURCES = $(SOURCES)

libbgpstream_formats_la_LIBADD = $(LIBS)

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
