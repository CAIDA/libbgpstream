#
# This file is part of bgpstream
#
# CAIDA, UC San Diego
# bgpstream-info@caida.org
#
# Copyright (C) 2012 The Regents of the University of California.
# Authors: Alistair King, Chiara Orsini
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 2 of the License, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program.  If not, see <http://www.gnu.org/licenses/>.
#

DOXYGEN_CONF = bgpstream.doxygen
DOXYGEN_FOLDERS = doxygen/html doxygen/man doxygen/latex
DOXYGEN_BIB_FILES = doxygen/bib*.aux doxygen/citelist.doc*

SPHINXOPTS    = -d sphinx/build/doctrees .
SPHINXBUILD   = sphinx-build
PAPER         =
BUILDDIR      = sphinx/build

EXTRA_DIST =

if HAS_DOXYGEN
bgpstream_doxygen=yes
endif

doxygen/html: doxy

doxygen/latex: doxy

doxygen/man: doxy

documentation: latex

latex: *.tex
	latex2html -dir html -split 0 *.tex

all: doxy sphinx

distclean: doxyclean
	rm -rf $(DOXYGEN_CONF)

clean: doxyclean

doxy: bgpstream.doxygen
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

sphinx: doxy
	@if test x"$(bgpstream_doxygen)" = xyes ; then \
		$(SPHINXBUILD) -b html $(SPHINXOPTS) $(BUILDDIR)/html ; \
		echo ; \
		echo "Build finished. The HTML pages are in $(BUILDDIR)/html." ; \
	else \
		echo "Sorry, you do not have doxygen in your PATH." ; \
	fi

doxyclean:
	rm -rf $(DOXYGEN_FOLDERS) $(DOXYGEN_BIB_FILES)

