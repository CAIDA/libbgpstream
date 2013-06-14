DOXYGEN_CONF = corsaro.doxygen
DOXYGEN_FOLDERS = doxygen/html doxygen/man doxygen/latex
DOXYGEN_BIB_FILES = doxygen/bib*.aux doxygen/citelist.doc*

EXTRA_DIST = doxygen/static

if HAS_DOXYGEN
EXTRA_DIST += $(DOXYGEN_FOLDERS)
corsaro_doxygen=yes
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
	    if test x"$(corsaro_doxygen)" = xyes ; then \
	      if test -f $(DOXYGEN_CONF) ; then \
	        CORSARO_VERSION=@CORSARO_MAJOR_VERSION@.@CORSARO_MID_VERSION@.@CORSARO_MINOR_VERSION@ doxygen $(DOXYGEN_CONF) ; \
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
#	@for folder in $(DOXYGEN_FOLDERS) ; do \
#	  if test -d $$folder ; then \
#	    rm -rf $$folder ; \
#	  fi \
#	done
