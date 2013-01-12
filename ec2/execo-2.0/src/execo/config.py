# Copyright 2009-2012 INRIA Rhone-Alpes, Service Experimentation et
# Developpement
#
# This file is part of Execo.
#
# Execo is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Execo is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
# License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Execo.  If not, see <http://www.gnu.org/licenses/>

import logging
import os
import sys

SSH = 0
SCP = 1
TAKTUK = 2

def checktty(f):
    try:
        if (get_ipython().__class__.__name__ == 'ZMQInteractiveShell' #@UndefinedVariable
            and f.__class__.__module__ == 'IPython.zmq.iostream'
            and f.__class__.__name__ == 'OutStream'):
            return True
    except NameError:
        pass
    if hasattr(f, 'isatty'):
        return f.isatty()
    if hasattr(f, 'fileno'):
        return os.isatty(f.fileno())
    return False

# _STARTOF_ configuration
configuration = {
    'log_level': logging.WARNING,
    'remote_tool': SSH,
    'fileput_tool': SCP,
    'fileget_tool': SCP,
    'compact_output_threshold': 4096,
    'kill_timeout': 5,
    'color_mode': checktty(sys.stdout)
                  and checktty(sys.stderr),
    'style_log_header': ('yellow',),
    'style_log_level' : ('magenta',),
    'style_object_repr': ('blue', 'bold'),
    'style_emph': ('magenta', 'bold'),
    'style_report_warn': ('magenta',),
    'style_report_error': ('red', 'bold'),
    }
# _ENDOF_ configuration
"""Global execo configuration parameters.

- ``log_level``: the log level (see module `logging`)

- ``remote_tool``: default tool to use when instanciating remote
  processes. Can be `execo.config.SSH` or `execo.config.TAKTUK`

- ``fileput_tool``: default tool to use to put files remotely. Can be
  `execo.config.SCP` or `execo.config.TAKTUK`

- ``fileget_tool``: default tool to use to get remote files. Can be
  `execo.config.SCP` or `execo.config.TAKTUK`

- ``compact_output_threshold``: only beginning and end of stdout /
  stderr are displayed by `execo.process.ProcessBase.dump` when their
  size is greater than this threshold. 0 for no threshold

- ``kill_timeout``: number of seconds to wait after a clean SIGTERM
  kill before assuming that the process is not responsive and killing
  it with SIGKILL

- ``color_mode``: whether to colorize output (with ansi escape
  sequences)

- ``style_log_header``, ``style_log_level``, ``style_object_repr``,
  ``style_emph``, ``style_report_warn``, ``style_report_error``:
  iterables of ansi attributes identifiers (those found in
  `execo.log._ansi_styles`)

"""

def make_default_connexion_params():
# _STARTOF_ default_connexion_params
    default_connexion_params = {
        'user':        None,
        'keyfile':     None,
        'port':        None,
        'ssh':         'ssh',
        'scp':         'scp',
        'taktuk':      'taktuk',
        'ssh_options': ( '-tt',
                         '-o', 'BatchMode=yes',
                         '-o', 'PasswordAuthentication=no',
                         '-o', 'StrictHostKeyChecking=no',
                         '-o', 'UserKnownHostsFile=/dev/null',
                         '-o', 'ConnectTimeout=20' ),
        'scp_options': ( '-o', 'BatchMode=yes',
                         '-o', 'PasswordAuthentication=no',
                         '-o', 'StrictHostKeyChecking=no',
                         '-o', 'UserKnownHostsFile=/dev/null',
                         '-o', 'ConnectTimeout=20',
                         '-rp' ),
        'taktuk_options': ( '-s', ),
        'taktuk_connector': 'ssh',
        'taktuk_connector_options': ( '-o', 'BatchMode=yes',
                                      '-o', 'PasswordAuthentication=no',
                                      '-o', 'StrictHostKeyChecking=no',
                                      '-o', 'UserKnownHostsFile=/dev/null',
                                      '-o', 'ConnectTimeout=20'),
        'taktuk_gateway': None,
        'taktuk_gateway_connexion_params': None,
        'pty': False,
        'host_rewrite_func': None
        }
# _ENDOF_ default_connexion_params
    return default_connexion_params

default_connexion_params = make_default_connexion_params()
"""Default connexion params for ``ssh``/``scp``/``taktuk`` connexions.

- ``user``: the user to connect with.

- ``keyfile``: the keyfile to connect with.

- ``port``: the port to connect to.

- ``ssh``: the ssh or ssh-like command.

- ``scp``: the scp or scp-like command.

- ``taktuk``: the taktuk command.

- ``ssh_options``: tuple of options passed to ssh.

- ``scp_options``: tuple of options passed to scp.

- ``taktuk_options``: tuple of options passed to taktuk.

- ``taktuk_connector``: the ssh-like connector command for taktuk.

- ``taktuk_connector_options``: tuple of options passed to
  taktuk_connector.

- ``taktuk_gateway``: hostname or `execo.host.Host` instance on which
  to launch the taktuk command.

- ``taktuk_gateway_connexion_params``: connexion parameters (if
  needed) to connect to taktuk_gateway.

- ``pty``: boolean. Wether to allocate or not a pty for
  ssh/scp.

- ``host_rewrite_func``: function called to rewrite hosts
  addresses. Takes a host address, returns a host address.
"""

def make_connexion_params(connexion_params = None, default_params = None):
    return_params = make_default_connexion_params()
    if default_params == None:
        default_params = default_connexion_params
    return_params.update(default_params)
    if connexion_params: return_params.update(connexion_params)
    return return_params

def load_configuration(filename, dicts_confs):
    """Update dicts with those found in file.

    :param filename: file to load dicts from
    
    :param dicts_confs: an iterable of couples (dict, string)

    Used to read configuration dicts. For each couple (dict, string),
    if a dict named string is defined in <file>, update
    dict with the content of this dict. Does nothing if unable to open
    <file>.
    """
    if os.path.isfile(filename):
        jailed_globals = {}
        try:
            execfile(filename, jailed_globals)
        except Exception, exc: #IGNORE:W0703
            print "ERROR while reading config file %s:" % (filename,)
            print exc
        for (dictio, conf) in dicts_confs:
            if jailed_globals.has_key(conf):
                dictio.update(jailed_globals[conf])

def get_user_config_filename():
    _user_conf_file = None
    if os.environ.has_key('HOME'):
        _user_conf_file = os.environ['HOME'] + '/.execo.conf.py'
    return _user_conf_file
    
load_configuration(
  get_user_config_filename(),
  ((configuration, 'configuration'),
   (default_connexion_params, 'default_connexion_params')))
