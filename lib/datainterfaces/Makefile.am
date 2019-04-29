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

SUBDIRS=

# to allow #include <config.h>
AM_CPPFLAGS= 	-I$(top_srcdir) \
		-I$(top_srcdir)/lib \
		-I$(top_srcdir)/lib/utils \
	 	-I$(top_srcdir)/common


noinst_LTLIBRARIES = libbgpstream-data-interfaces.la

DI_SOURCES=
DI_LIBS=

if WITH_DATA_INTERFACE_BROKER
DI_SOURCES+=bsdi_broker.c \
	    bsdi_broker.h
endif

if WITH_DATA_INTERFACE_SINGLEFILE
DI_SOURCES+=bsdi_singlefile.c \
	    bsdi_singlefile.h
endif

if WITH_DATA_INTERFACE_KAFKA
DI_SOURCES+=bsdi_kafka.c \
	    bsdi_kafka.h
endif

if WITH_DATA_INTERFACE_CSVFILE
DI_SOURCES+=bsdi_csvfile.c \
	    bsdi_csvfile.h
endif

if WITH_DATA_INTERFACE_SQLITE
DI_SOURCES+=bsdi_sqlite.c \
	    bsdi_sqlite.h
endif

if WITH_DATA_INTERFACE_BETABMP
DI_SOURCES+=bsdi_betabmp.c \
	    bsdi_betabmp.h
endif

if WITH_DATA_INTERFACE_BETARISLIVE
DI_SOURCES+=bsdi_betarislive.c \
	    bsdi_betarislive.h
endif

libbgpstream_data_interfaces_la_SOURCES = $(DI_SOURCES)

libbgpstream_data_interfaces_la_LIBADD = $(DI_LIBS)

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
