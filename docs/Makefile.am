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

DOXYGEN_CONF = bgpstream.doxygen
DOXYGEN_FOLDERS = doxygen/html doxygen/man doxygen/latex
DOXYGEN_BIB_FILES = doxygen/bib*.aux doxygen/citelist.doc*

EXTRA_DIST = doxygen/static

if HAS_DOXYGEN
EXTRA_DIST += $(DOXYGEN_FOLDERS)
bgpstream_doxygen=yes
endif

doxygen/html: doxy

doxygen/latex: doxy

doxygen/man: doxy

documentation: latex

latex: *.tex
	latex2html -dir html -split 0 *.tex

all: doxy

distclean: doxyclean
	rm -rf $(DOXYGEN_CONF)

clean: doxyclean

doxy:
	@for folder in $(DOXYGEN_FOLDERS) ; do \
	  if test -d $$folder ; then \
	    break ; \
	  else \
	    if test x"$(bgpstream_doxygen)" = xyes ; then \
	      if test -f $(DOXYGEN_CONF) ; then \
	        BGPSTREAM_VERSION=@BGPSTREAM_MAJOR_VERSION@.@BGPSTREAM_MID_VERSION@.@BGPSTREAM_MINOR_VERSION@ doxygen $(DOXYGEN_CONF) ; \
	      else \
	        echo "Sorry, doxygen conf file $(DOXYGEN_CONF) does not exist" ; \
	      fi ; \
	    else \
	      echo "Sorry, you do not have doxygen in your PATH." ; \
	    fi ; \
	  fi \
	done

doxyclean:
	rm -rf $(DOXYGEN_FOLDERS) $(DOXYGEN_BIB_FILES)

