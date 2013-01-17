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

from execo.action import ActionFactory
from execo.config import SSH, SCP
from execo.host import Host
from execo_g5k.config import g5k_configuration
import execo
import itertools
import re
import socket

frontend_factory = ActionFactory(remote_tool = SSH,
                                 fileput_tool = SCP,
                                 fileget_tool = SCP)

__default_frontend = None
__default_frontend_cached = False
def get_default_frontend():
    """Return the name of the default frontend."""
    global __default_frontend, __default_frontend_cached #IGNORE:W0603
    if not __default_frontend_cached:
        __default_frontend_cached = True
        if g5k_configuration.get("default_frontend"):
            __default_frontend = g5k_configuration["default_frontend"]
        else:
            try:
                localhost = socket.gethostname()
            except socket.error:
                localhost = ""
            mo = re.search("^[^ \t\n\r\f\v\.]+\.([^ \t\n\r\f\v\.]+)\.grid5000.fr$", localhost)
            if mo:
                __default_frontend = mo.group(1)
            else:
                __default_frontend = None
    return __default_frontend

def get_frontend_host(frontend = None):
    """Given a frontend name, or None, and based on the global configuration, returns the frontend to connect to or None."""
    if frontend == None:
        frontend = get_default_frontend()
    if g5k_configuration.get('no_ssh_for_local_frontend') == True and frontend == get_default_frontend():
        frontend = None
    if frontend:
        frontend = Host(frontend)
    return frontend

__g5k_host_group_regex = re.compile("^([a-zA-Z]+)-\d+\.([a-zA-Z]+)\.grid5000\.fr$")

def g5k_host_get_cluster(host):
    if isinstance(host, execo.Host):
        host = host.address
    m = __g5k_host_group_regex.match(host)
    if m: return m.group(1)
    else: return None

def g5k_host_get_site(host):
    if isinstance(host, execo.Host):
        host = host.address
    m = __g5k_host_group_regex.match(host)
    if m: return m.group(2)
    else: return None

def group_hosts(nodes):
    grouped_nodes = {}
    for site, site_nodes in itertools.groupby(
        sorted(nodes,
               lambda n1, n2: cmp(
                   g5k_host_get_site(n1),
                   g5k_host_get_site(n2))),
        g5k_host_get_site):
        grouped_nodes[site] = {}
        for cluster, cluster_nodes in itertools.groupby(
            sorted(site_nodes,
                   lambda n1, n2: cmp(
                       g5k_host_get_cluster(n1),
                       g5k_host_get_cluster(n2))),
            g5k_host_get_cluster):
            grouped_nodes[site][cluster] = list(cluster_nodes)
    return grouped_nodes
