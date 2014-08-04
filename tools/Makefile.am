

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/lib 


bin_PROGRAMS =  bgpdump example

bgpdump_SOURCES = bgpdump.c
bgpdump_LDADD = -lbgpdump
bgpdump_LDFLAGS = -L$(top_builddir)/lib

example_SOURCES = example.c
example_LDADD = -lbgpdump
example_LDFLAGS = -L$(top_builddir)/lib


ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
