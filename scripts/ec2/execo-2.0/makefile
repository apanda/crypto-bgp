# Copyright 2009-2011 INRIA Rhone-Alpes, Service Experimentation et
# Developpement
# This file is part of Execo, released under the GNU Lesser Public
# License, version 3 or later.

.PHONY: all build install doc cleandoc sphinxdoccommon sphinxdochtml sphinxdoclatex cleansphinxdoc epydoc cleanepydoc check clean dist

PREFIX=/usr/local

all: build

build: execo.conf.py.sample execo-run
	python setup.py build

install: build
	python setup.py install --prefix=$(PREFIX)

doc: sphinxdochtml

cleandoc: cleansphinxdoc cleanepydoc

sphinxdochtml:
	mkdir -p doc/_template
	sphinx-build -b html doc doc/_build/html

cleansphinxdoc:
	rm -rf doc/_build/ doc/_template doc/_templates/

epydoc: epydoc/redirect.html

epydoc/redirect.html: src
	epydoc -v --docformat "restructuredtext en" --parse-only --html --output=epydoc src/execo src/execo_g5k src/execo_engine

cleanepydoc:
	rm -rf epydoc

check:
#TODO

clean: cleandoc
	rm -rf build dist MANIFEST execo.conf.py.sample execo-run
	find . -name '*.pyc' -exec $(RM) {} \;

dist: doc
	python setup.py sdist

extract = ( sed -n '/^\# _STARTOF_ $(2)/,/^\# _ENDOF_ $(2)/p' $(1) | grep -v ^\# | python -c 'import sys, textwrap; print textwrap.dedent(sys.stdin.read())' | sed 's/^\(.*\)$$/\# \1/' ; echo )

execo.conf.py.sample: execo.conf.py.sample.in src/execo/config.py src/execo_g5k/config.py
	cp $< $@
	$(call extract,src/execo/config.py,configuration) >> $@
	$(call extract,src/execo/config.py,default_connexion_params) >> $@
	$(call extract,src/execo_g5k/config.py,g5k_configuration) >> $@
	$(call extract,src/execo_g5k/config.py,default_frontend_connexion_params) >> $@
	$(call extract,src/execo_g5k/config.py,default_oarsh_oarcp_params) >> $@

do_subst = sed -e 's,[@]prefix[@],$(PREFIX),g'

execo-run: execo-run.in
	$(do_subst) < $< > $@
	chmod a+x execo-run
