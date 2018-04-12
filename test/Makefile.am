#
# Copyright (C) 2015 The Regents of the University of California.
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

AM_CPPFLAGS = 	-I$(top_srcdir) \
	 	-I$(top_srcdir)/lib \
	 	-I$(top_srcdir)/lib/utils \
	 	-I$(top_srcdir)/common

# Run bgpstream-test-rpki only if WITH_RPKI is set
if WITH_RPKI
RPKI_TEST=bgpstream-test-rpki
else
RPKI_TEST=
endif

TESTS = 				\
	bgpstream-test 			\
	bgpstream-test-filters		\
	bgpstream-test-utils-addr 	\
	bgpstream-test-utils-pfx	\
	bgpstream-test-utils-patricia 			\
  $(RPKI_TEST)

check_PROGRAMS =  			\
	bgpstream-test 			\
	bgpstream-test-filters		\
	bgpstream-test-utils-addr 	\
	bgpstream-test-utils-pfx	\
	bgpstream-test-utils-patricia  \
  $(RPKI_TEST)

bgpstream_test_SOURCES = bgpstream-test.c bgpstream_test.h
bgpstream_test_LDADD   = $(top_builddir)/lib/libbgpstream.la

bgpstream_test_filters_SOURCES = bgpstream-test-filters.c bgpstream_test.h
bgpstream_test_filters_LDADD   = $(top_builddir)/lib/libbgpstream.la

bgpstream_test_rpki_SOURCES = bgpstream-test-rpki.c bgpstream_test.h
bgpstream_test_rpki_LDADD   = $(top_builddir)/lib/libbgpstream.la

bgpstream_test_utils_addr_SOURCES = bgpstream-test-utils-addr.c bgpstream_test.h
bgpstream_test_utils_addr_LDADD   = $(top_builddir)/lib/libbgpstream.la

bgpstream_test_utils_pfx_SOURCES = bgpstream-test-utils-pfx.c bgpstream_test.h
bgpstream_test_utils_pfx_LDADD   = $(top_builddir)/lib/libbgpstream.la

bgpstream_test_utils_patricia_SOURCES = bgpstream-test-utils-patricia.c bgpstream_test.h
bgpstream_test_utils_patricia_LDADD   = $(top_builddir)/lib/libbgpstream.la

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~



