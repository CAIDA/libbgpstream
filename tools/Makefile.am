

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/lib 


bin_PROGRAMS =  bgpdump bgpdump_example

bgpdump_SOURCES = bgpdump.c
bgpdump_LDADD = -lbgpdump
bgpdump_LDFLAGS = -L$(top_builddir)/lib

bgpdump_example_SOURCES = bgpdump_example.c
bgpdump_example_LDADD = -lbgpdump
bgpdump_example_LDFLAGS = -L$(top_builddir)/lib

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = *~
