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

all:

docs: doxy sphinx

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

.PHONY: doxy sphinx
