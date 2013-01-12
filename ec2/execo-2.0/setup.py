#!/usr/bin/env python

# Copyright 2009-2011 INRIA Rhone-Alpes, Service Experimentation et
# Developpement
# This file is part of Execo, released under the GNU Lesser Public
# License, version 3 or later.

from distutils.core import setup
import sys

setup(name = 'execo',
      license = 'GNU GPL v3',
      version = '2.0',
      description = 'API for parallel local or remote processes execution',
      long_description = 'This module offers a high level API for parallel'
      'local or remote processes execution with the `Action` class hierarchy,'
      'and a lower level API with the `Process` class, for handling individual'
      'subprocesses.',
      author = 'Matthieu Imbert',
      author_email = 'matthieu.imbert@inria.fr',
      url = 'http://execo.gforge.inria.fr',
      package_dir = {'': 'src'},
      packages = [ 'execo', 'execo_g5k', 'execo_engine' ],
      scripts = [ 'execo-run' ],
      classifiers = [ 'Development Status :: 4 - Beta',
                      'Environment :: Console',
                      'Intended Audience :: Developers',
                      'Intended Audience :: Information Technology',
                      'Intended Audience :: Science/Research',
                      'Intended Audience :: System Administrators',
                      'Operating System :: POSIX :: Linux',
                      'Programming Language :: Python :: 2.5',
                      'Topic :: Software Development',
                      'Topic :: System :: Clustering',
                      'Topic :: System :: Distributed Computing'],
      )
