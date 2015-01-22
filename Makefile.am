#
# libbgpdump-caida
#
# Alistair King, CAIDA, UC San Diego
# corsaro-info@caida.org
#
# Copyright (C) 2012 The Regents of the University of California.
#
# This file is part of libbgpdump-caida.
#

SUBDIRS=

# to allow #include <config.h>
AM_CPPFLAGS= -I$(top_srcdir)

noinst_LTLIBRARIES = libbgpdump-caida.la

if WITH_WANDIO
IO_SOURCE=bgpdump_cfile_tools_wandio.c
else
IO_SOURCE=bgpdump_cfile_tools.c
endif

libbgpdump_la_SOURCES = bgpdump_attr.h    \
			bgpdump_formats.h \
			bgpdump_lib.h     \
			bgpdump_mstream.h \
			bgpdump_cfile_tools.h     \
			bgpdump_util.h    \
			bgpdump_lib.c     \
			bgpdump_mstream.c \
			$(IO_SOURCE)	  \
			bgpdump_util.c

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
