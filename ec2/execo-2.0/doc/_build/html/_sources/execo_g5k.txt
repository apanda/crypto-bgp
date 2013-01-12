****************
:mod:`execo_g5k`
****************

.. automodule:: execo_g5k

To use execo_g5k, passwordless, public-key based authentification must
be used (either with appropriate public / private specific g5k keys
shared on all your home directories, or with an appropriate ssh-agent
forwarding configuration).

The code can be run from a frontend, from a computer outside of
Grid5000 (for this you need an appropriate configuration, see section
`Running from outside Grid5000`_) or from g5k nodes (in this case, you
need to explicitely refer to the local Grid5000 site / frontend by
name, and the node running the code needs to be able to connect to all
involved frontends)

OAR functions
=============

OarSubmission
-------------
.. autoclass:: execo_g5k.oar.OarSubmission
   :members:

oarsub
------
.. autofunction:: execo_g5k.oar.oarsub

oardel
------
.. autofunction:: execo_g5k.oar.oardel

get_current_oar_jobs
--------------------
.. autofunction:: execo_g5k.oar.get_current_oar_jobs

get_oar_job_info
----------------
.. autofunction:: execo_g5k.oar.get_oar_job_info

wait_oar_job_start
------------------
.. autofunction:: execo_g5k.oar.wait_oar_job_start

get_oar_job_nodes
-----------------
.. autofunction:: execo_g5k.oar.get_oar_job_nodes

get_oar_job_subnets
-------------------
.. autofunction:: execo_g5k.oar.get_oar_job_subnets

OARGRID functions
=================

oargridsub
----------
.. autofunction:: execo_g5k.oargrid.oargridsub

oargriddel
----------
.. autofunction:: execo_g5k.oargrid.oargriddel

get_current_oargrid_jobs
------------------------
.. autofunction:: execo_g5k.oargrid.get_current_oargrid_jobs

get_oargrid_job_info
--------------------
.. autofunction:: execo_g5k.oargrid.get_oargrid_job_info

get_oargrid_job_oar_jobs
------------------------
.. autofunction:: execo_g5k.oargrid.get_oargrid_job_oar_jobs

wait_oargrid_job_start
----------------------
.. autofunction:: execo_g5k.oargrid.wait_oargrid_job_start

get_oargrid_job_nodes
---------------------
.. autofunction:: execo_g5k.oargrid.get_oargrid_job_nodes

kadeploy3
=========

Deployment
----------
.. autoclass:: execo_g5k.kadeploy.Deployment
   :members:

Kadeployer
----------
.. inheritance-diagram:: execo_g5k.kadeploy.Kadeployer
.. autoclass:: execo_g5k.kadeploy.Kadeployer
   :members:
   :show-inheritance:

kadeploy
--------
.. autofunction:: execo_g5k.kadeploy.kadeploy

deploy
-------------------
.. autofunction:: execo_g5k.kadeploy.deploy

Utilities
=========

format_oar_date
---------------
.. autofunction:: execo_g5k.oar.format_oar_date

format_oar_duration
-------------------
.. autofunction:: execo_g5k.oar.format_oar_duration

oar_date_to_unixts
------------------
.. autofunction:: execo_g5k.oar.oar_date_to_unixts

oar_duration_to_seconds
-----------------------
.. autofunction:: execo_g5k.oar.oar_duration_to_seconds

Grid5000 API utilities
======================

.. automodule:: execo_g5k.api_utils

get_g5k_sites
-------------
.. autofunction:: get_g5k_sites

get_site_clusters
-----------------
.. autofunction:: get_site_clusters

get_cluster_hosts
-----------------
.. autofunction:: get_cluster_hosts

get_g5k_clusters
----------------
.. autofunction:: get_g5k_clusters

get_g5k_hosts
-------------
.. autofunction:: get_g5k_hosts

get_cluster_site
----------------
.. autofunction:: get_cluster_site

Configuration
=============

This module may be configured at import time by defining two dicts
`g5k_configuration` and `default_frontend_connexion_params` in the
file ``~/.execo.conf.py``

The `g5k_configuration` dict contains global g5k configuration
parameters.

.. autodata:: execo_g5k.config.g5k_configuration

Its default values are:

.. literalinclude:: ../src/execo_g5k/config.py
   :start-after: # _STARTOF_ g5k_configuration
   :end-before: # _ENDOF_ g5k_configuration
   :language: python

The `default_frontend_connexion_params` dict contains default
parameters for remote connexions to grid5000 frontends.

.. autodata:: execo_g5k.config.default_frontend_connexion_params

Its default values are:

.. literalinclude:: ../src/execo_g5k/config.py
   :start-after: # _STARTOF_ default_frontend_connexion_params
   :end-before: # _ENDOF_ default_frontend_connexion_params
   :language: python

in default_frontend_connexion_params, the ``host_rewrite_func``
configuration variable is set to automatically map a site name to its
corresponding frontend, so that all commands are run on the proper
frontends.

`default_oarsh_oarcp_params` contains default connexion parameters
suitable to connect to grid5000 nodes with oarsh / oarcp.

.. autodata:: execo_g5k.config.default_oarsh_oarcp_params

Its default values are:

.. literalinclude:: ../src/execo_g5k/config.py
   :start-after: # _STARTOF_ default_oarsh_oarcp_params
   :end-before: # _ENDOF_ default_oarsh_oarcp_params
   :language: python

OAR keys
========

Oar/oargrid by default generate job specific ssh keys. So by default,
one has to retrieve these keys and explicitely use them for connecting
to the jobs, which is painfull. Another possibility is to tell
oar/oargrid to use specific keys. Oar can automatically use the key
pointed to by the environement variable ``OAR_JOB_KEY_FILE`` if it is
defined. Oargrid does not automatically use this key, but execo also
takes care of explicitely telling oargrid to use it if it is
defined. So the most convenient way to use execo/oar/oargrid, is to
set ``OAR_JOB_KEY_FILE`` in your ``~/.profile`` to point to your
internal Grid5000 ssh key and export this environment variable, or use
the ``oar_job_key_file`` in `execo_g5k.config.g5k_configuration`.

Running from another host than a frontend
=========================================

Note that when running a script from another host than a frontend
(from a node or from your laptop outside Grid5000), everything will
work except ``oarsh`` / ``oarcp`` connexions, since these executables
only exist on frontends. In this case you can still connect by using
``ssh`` / ``scp`` as user ``oar`` on port 6667 (ie. use
``connexion_params = {'user': 'oar', 'port': 6667}``). This is the
port where an oar-specific ssh server is listening. This server will
then change user from oar to the user currently owning the node.

Running from outside Grid5000
=============================

Execo scripts can be run from outside grid5000 with a subtle
configuration of both execo and ssh.

First, in ``~/.ssh/config``, declare aliases for g5k connexion
(through the access machine). For example, here is an alias ``g5k``
for connecting through the lyon access::

 Host *.g5k g5k
   ProxyCommand ssh access.grid5000.fr "nc -q 0 `echo %h | sed -ne 's/\.g5k$//p;s/^g5k$/lyon/p'` %p"

Then in ``~/.execo.conf.py`` put this code::

 import re

 default_connexion_params = {
     'host_rewrite_func': lambda host: re.sub("\.grid5000\.fr$", ".g5k", host),
     'taktuk_gateway': 'g5k'
     }


 default_frontend_connexion_params = {
     'host_rewrite_func': lambda host: host + ".g5k"
     }

 g5k_configuration = {
     'api_username': '<g5k_username>',
     }

Now, every time execo tries to connect to a host, the host name is
rewritten as to be reached through the Grid5000 ssh proxy connexion
alias, and the same for the frontends.

.. _execo_g5k-perfect_configuration:

The perfect grid5000 connexion configuration
============================================

* use separate ssh keys for connecting from outside to grid5000 and
  inside grid5000:

  * use your regular ssh key for connecting from outside grid5000 to
    inside grid5000 by adding your regular public key to
    ``~/.ssh/authorized_keys`` on each site's nfs.

  * generate (with ``ssh-keygen``)a specific grid5000 private/public
    key pair, without passphrase, for navigating inside
    grid5000. replicate ``~/.ssh/id_dsa`` and ``~/.ssh/id_dsa.pub``
    (for a dsa key pair, or the equivalent rsa keys for an rsa key
    pair) to ``~/.ssh/`` on each site's nfs and add
    ``~/.ssh/id_dsa.pub`` to ``~/.ssh/authorized_keys`` on each site's
    nfs.

* add ``export OAR_JOB_KEY_FILE=~/.ssh/id_dsa`` (or id_rsa) to each
  site's ``~/.bash_profile``

* Connexions should then work directly with oarsh/oarcp if you use
  `execo_g5k.config.default_oarsh_oarcp_params` connexion
  parameters. Connexions should also work directly with ssh for nodes
  reserved with the ``allow_classic_ssh`` option. Finally, for
  deployed nodes, connexions should work directly because option
  ``-k`` is passed to kadeploy3 by default.

TODO: Currently, due to an ongoing bug or misconfiguration (see
https://www.grid5000.fr/cgi-bin/bugzilla3/show_bug.cgi?id=3302), oar
fails to access the ssh keys if they are not world-readable, so you
need to make them so.
