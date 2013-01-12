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

from config import g5k_configuration
from execo.action import Remote, ActionNotificationProcessLifecycleHandler, \
    Action, get_remote
from execo.config import make_connexion_params
from execo.host import get_hosts_set, Host
from execo.log import set_style, logger
from execo.process import ProcessOutputHandler, get_process
from execo.time_utils import format_seconds
from execo.utils import comma_join
from execo_g5k.config import default_frontend_connexion_params
from execo_g5k.utils import get_frontend_host
from utils import get_default_frontend
import copy
import re
import time

class Deployment(object):
    """A kadeploy3 deployment.

    POD set_style class.

    members are:

    - hosts: iterable of `execo.host.Host` on which to deploy.

    - env_file:

    - env_name:

    - user:

    - other_options:

    there must be either one of env_name or env_file parameter
    given. If none given, will try to use the default environement
    from `g5k_configuration`.
    """

    def __init__(self,
                 hosts = None,
                 env_file = None,
                 env_name = None,
                 user = None,
                 other_options = None):
        self.hosts = hosts
        self.env_file = env_file
        self.env_name = env_name
        self.user = user
        self.other_options = other_options

    def _get_common_kadeploy_command_line(self):
        cmd_line = g5k_configuration.get('kadeploy3')
        cmd_line += " " + g5k_configuration.get('kadeploy3_options')
        if self.env_file and self.env_name:
            raise ValueError, "Deployment cannot have both env_file and env_name"
        if (not self.env_file) and (not self.env_name):
            if g5k_configuration.get('default_env_name') and g5k_configuration.get('default_env_file'):
                raise Exception, "g5k_configuration cannot have both default_env_name and default_env_file"
            if (not g5k_configuration.get('default_env_name')) and (not g5k_configuration.get('default_env_file')):
                raise Exception, "no environment name or file found"
            if g5k_configuration.get('default_env_name'):
                cmd_line += " -e %s" % (g5k_configuration['default_env_name'],)
            elif g5k_configuration.get('default_env_file'):
                cmd_line += " -a %s" % (g5k_configuration['default_env_file'],)
        elif self.env_name:
            cmd_line += " -e %s" % (self.env_name,)
        elif self.env_file:
            cmd_line += " -a %s" % (self.env_file,)
        if self.user != None:
            cmd_line += " -u %s" % (self.user,)
        if self.other_options:
            cmd_line += " %s" % (self.other_options,)
        return cmd_line

    def __repr__(self):
        s = ""
        if self.hosts != None: s = comma_join(s, "hosts=%r" % (self.hosts,))
        if self.env_file != None: s = comma_join(s, "env_file=%r" % (self.env_file,))
        if self.env_name != None: s = comma_join(s, "env_name=%r" % (self.env_name,))
        if self.user != None: s = comma_join(s, "user=%r" % (self.user,))
        if self.other_options: s = comma_join(s, "other_options=%r" % (self.other_options,))
        return "Deployment(%s)" % (s,)

class _KadeployStdoutHandler(ProcessOutputHandler):

    """Parse kadeploy3 stdout."""
    
    def __init__(self, kadeployer, out = False):
        """
        :param kadeployer: the `execo_g5k.kadeploy.Kadeployer` to
          which this `execo.process.ProcessOutputHandler` is attached.
        """
        super(_KadeployStdoutHandler, self).__init__()
        self._kadeployer = kadeployer
        self._good_nodes_header_re = re.compile("^Nodes correctly deployed on cluster \w+\s*$")
        self._bad_nodes_header_re = re.compile("^Nodes not correctly deployed on cluster \w+\s*$")
        self._good_node_re = re.compile("^(\S+)\s*$")
        self._bad_node_re = re.compile("^(\S+)(\s+\(.*\))?\s*$")
        self._SECTION_NONE, self._SECTION_GOODNODES, self._SECTION_BADNODES = range(3)
        self._current_section = self._SECTION_NONE
        self._out = out

    def action_reset(self):
        self._current_section = self._SECTION_NONE
        
    def read_line(self, process, string, eof = False, error = False):
        if self._out:
            print string,
        if self._good_nodes_header_re.search(string) != None:
            self._current_section = self._SECTION_GOODNODES
            return
        if self._bad_nodes_header_re.search(string) != None:
            self._current_section = self._SECTION_BADNODES
            return
        if self._current_section == self._SECTION_GOODNODES:
            so = self._good_node_re.search(string)
            if so != None:
                host_address = so.group(1)
                self._kadeployer._add_good_host_address(host_address)
        elif self._current_section == self._SECTION_BADNODES:
            so = self._bad_node_re.search(string)
            if so != None:
                host_address = so.group(1)
                self._kadeployer._add_bad_host_address(host_address)

class _KadeployStderrHandler(ProcessOutputHandler):

    """Parse kadeploy3 stderr."""
    
    def __init__(self, kadeployer, out = False):
        """
        :param kadeployer: the `execo_g5k.kadeploy.Kadeployer` to
          which this `execo.process.ProcessOutputHandler` is attached.
        """
        super(_KadeployStderrHandler, self).__init__()
        self._kadeployer = kadeployer
        self._out = out

    def read_line(self, process, string, eof = False, error = False):
        if self._out:
            print string,

class Kadeployer(Remote):

    """Deploy an environment with kadeploy3 on several hosts.

    Able to deploy in parallel to multiple frontends.
    """

    def __init__(self, deployment, frontend_connexion_params = None, out = False, name = None, **kwargs):
        """
        :param deployment: instance of Deployment class describing the
          intended kadeployment.

        :param frontend_connexion_params: connexion params for
          connecting to frontends if needed. Values override those in
          `execo_g5k.config.default_frontend_connexion_params`.

        :param out: if True, output kadeploy stdout / stderr to
          stdout.

        :param name: action's name

        :param kwargs: passed to the instanciated processes.
        """
        super(Remote, self).__init__(name = name)
        self._good_hosts = set()
        self._bad_hosts = set()
        self._frontend_connexion_params = frontend_connexion_params
        self._deployment = deployment
        self._out = out
        self._other_kwargs = kwargs
        self._fhosts = get_hosts_set(deployment.hosts)
        searchre1 = re.compile("^[^ \t\n\r\f\v\.]+\.([^ \t\n\r\f\v\.]+)\.grid5000.fr$")
        searchre2 = re.compile("^[^ \t\n\r\f\v\.]+\.([^ \t\n\r\f\v\.]+)$")
        searchre3 = re.compile("^[^ \t\n\r\f\v\.]+$")
        frontends = dict()
        for host in self._fhosts:
            frontend = None
            mo1 = searchre1.search(host.address)
            if mo1 != None:
                frontend = mo1.group(1)
            else:
                mo2 = searchre2.search(host.address)
                if mo2 != None:
                    frontend = mo1.group(1)
                else:
                    mo3 = searchre3.search(host.address)
                    if mo3 != None:
                        frontend = get_default_frontend()
                    else:
                        raise ValueError, "unknown frontend for host %s" % host.address
            if frontends.has_key(frontend):
                frontends[frontend].append(host)
            else:
                frontends[frontend] = [host]
        self._processes = list()
        lifecycle_handler = ActionNotificationProcessLifecycleHandler(self, len(frontends))
        for frontend in frontends.keys():
            kadeploy_command = self._deployment._get_common_kadeploy_command_line()
            for host in frontends[frontend]:
                kadeploy_command += " -m %s" % (host.address,)
            p = get_process(kadeploy_command,
                            host = get_frontend_host(frontend),
                            connexion_params = make_connexion_params(frontend_connexion_params,
                                                                     default_frontend_connexion_params),
                            stdout_handler = _KadeployStdoutHandler(self, out = self._out),
                            stderr_handler = _KadeployStderrHandler(self, out = self._out),
                            process_lifecycle_handler = lifecycle_handler,
                            pty = True,
                            **self._other_kwargs)
            self._processes.append(p)

    def _common_reset(self):
        super(Kadeployer, self)._common_reset()
        self._good_hosts = set()
        self._bad_hosts = set()
        
    def _args(self):
        return [ repr(self._deployment) ] + Action._args(self) + Kadeployer._kwargs(self)

    def _kwargs(self):
        kwargs = []
        if self._frontend_connexion_params: kwargs.append("frontend_connexion_params=%r" % (self._frontend_connexion_params,))
        if self._out: kwargs.append("out=%r" % (self._out,))
        for (k, v) in self._other_kwargs.iteritems(): kwargs.append("%s=%s" % (k, v))
        return kwargs

    def _infos(self):
        return Remote._infos(self) + [ "cmds=%r" % ([ process.cmd() for process in self._processes],),
                                       "deployed_hosts=%r" % (self._good_hosts,),
                                       "error_hosts=%r" % (self._bad_hosts,) ]

    def name(self):
        if self._name == None:
            return "%s on %i hosts / %i frontends" % (self.__class__.__name__, len(self._fhosts), len(self._processes))
        else:
            return self._name

    def _add_good_host_address(self, host_address):
        """Add a host to the deployed hosts list. Intended to be called from the `execo.process.ProcessOutputHandler`."""
        self._good_hosts.add(Host(host_address))

    def _add_bad_host_address(self, host_address):
        """Add a host to the hosts not deployed list. Intended to be called from the `execo.process.ProcessOutputHandler`."""
        self._bad_hosts.add(Host(host_address))

    def get_deployed_hosts(self):
        """Return an iterable of `Host` containing the deployed hosts.

        this iterable won't be complete if
        `execo_g5k.kadeploy.Kadeployer` has not terminated.
        """
        return list(self._good_hosts)

    def get_error_hosts(self):
        """Return an iterable of `Host` containing the hosts not deployed.

        this iterable won't be complete if
        `execo_g5k.kadeploy.Kadeployer` has not terminated.
        """
        return list(self._fhosts.difference(self._good_hosts))

    def ok(self):
        ok = super(Kadeployer, self).ok()
        if self.ended():
            if len(self._good_hosts.intersection(self._bad_hosts)) != 0:
                ok = False
            if len(self._good_hosts.union(self._bad_hosts).symmetric_difference(self._fhosts)) != 0:
                ok = False
        return ok

    def reset(self):
        retval = super(Kadeployer, self).reset()
        for process in self._processes:
            process.stdout_handler().action_reset()
        return retval

def kadeploy(deployment, out = False, frontend_connexion_params = None, timeout = None):
    """Deploy hosts with kadeploy3.

    :param deployment: instance of Deployment class describing the
      intended kadeployment.

    :param out: if True, output kadeploy stdout / stderr to stdout.

    :param frontend_connexion_params: connexion params for connecting
      to frontends if needed. Values override those in
      `execo_g5k.config.default_frontend_connexion_params`.

    :param timeout: deployment timeout. None (which is the default
      value) means no timeout.

    Returns a tuple (iterable of `execo.host.Host` containing the
    deployed host, iterable of `execo.host.Host` containing the nodes
    not deployed).
    """
    kadeployer = Kadeployer(deployment,
                            out = out,
                            frontend_connexion_params = frontend_connexion_params,
                            timeout = timeout).run()
    if not kadeployer.ok():
        logoutput = set_style("deployment error:", 'emph') + " %s\n" % (kadeployer,) + set_style("kadeploy processes:\n", 'emph')
        for p in kadeployer.processes():
            logoutput += "%s\n" % (p,)
            logoutput += set_style("stdout:", 'emph') + "\n%s\n" % (p.stdout())
            logoutput += set_style("stderr:", 'emph') + "\n%s\n" % (p.stderr())
        logger.error(logoutput)
    return (kadeployer.get_deployed_hosts(), kadeployer.get_error_hosts())

def deploy(deployment,
           check_deployed_command = True,
           node_connexion_params = None,
           num_tries = 2,
           check_enough_func = None,
           out = False,
           frontend_connexion_params = None,
           deploy_timeout = None,
           check_timeout = 30,
           timeout = False):
    """Deploy nodes, many times if needed, checking which of these nodes are already deployed with a user-supplied command. If no command given for checking if nodes deployed, rely on kadeploy to know which nodes are deployed.

    - loop `num_tries` times:

      - if ``check_deployed_command`` given, try to connect to these
        hosts using the supplied `node_connexion_params` (or the
        default ones), and to execute ``check_deployed_command``. If
        connexion succeeds and the command returns 0, the host is
        assumed to be deployed, else it is assumed to be undeployed.

      - optionnaly call user-supplied ``check_enough_func``, passing
        to it the list of deployed and undeployed hosts, to let user
        code decide if enough nodes deployed. Otherwise, try as long
        as there are undeployed nodes.

      - deploy the undeployed nodes

    returns a tuple with the list of deployed hosts and the list of
    undeployed hosts.

    :param deployment: instance of `execo.kadeploy.Deployment` class
      describing the intended kadeployment.

    :param check_deployed_command: command to perform remotely to
      check node deployement. May be a String, True, False or None. If
      String: the actual command to be used (This command should
      return 0 if the node is correctly deployed, or another value
      otherwise). If True, the default command value will be used
      (from `execo_g5k.config.g5k_configuration`). If None or False,
      no check is made and deployed/undeployed status will be taken
      from kadeploy's output.

    :param node_connexion_params: a dict similar to
      `execo.config.default_connexion_params` whose values will
      override those in `execo.config.default_connexion_params` when
      connecting to check node deployment with
      ``check_deployed_command`` (see below).

    :param num_tries: number of deploy tries

    :param check_enough_func: a function taking as parameter a list of
      deployed hosts and a list of undeployed hosts, which will be
      called at each deployment iteration end, and that should return
      a boolean indicating if there is already enough nodes (in this
      case, no further deployement will be attempted).

    :param out: if True, output kadeploy stdout / stderr to stdout.

    :param frontend_connexion_params: connexion params for connecting
      to frontends if needed. Values override those in
      `execo_g5k.config.default_frontend_connexion_params`.

    :param deploy_timeout: timeout for deployement. Default is None,
      which means no timeout.

    :param check_timeout: timeout for node deployment checks. Default
      is 30 seconds.

    :param timeout: timeout for g5k operations, except deployment.
      Default is False, which means use
      ``execo_g5k.config.g5k_configuration['default_timeout']``. None
      means no timeout.
    """

    if timeout == False:
        timeout = g5k_configuration.get('default_timeout')

    if check_enough_func == None:
        check_enough_func = lambda deployed, undeployed: len(undeployed) == 0

    if check_deployed_command == True:
        check_deployed_command = g5k_configuration.get('check_deployed_command')

    def check_update_deployed(deployed_hosts, undeployed_hosts, check_deployed_command, node_connexion_params): #IGNORE:W0613
        logger.info(set_style("check which hosts are already deployed among:", 'emph') + " %s", undeployed_hosts)
        deployed_check = get_remote(check_deployed_command,
                                    undeployed_hosts,
                                    connexion_params = node_connexion_params,
                                    log_exit_code = False,
                                    log_timeout = False,
                                    log_error = False,
                                    timeout = check_timeout)
        deployed_check.run()
        newly_deployed = list()
        for process in deployed_check.processes():
            logger.debug(set_style("check on %s:" % (process.host(),), 'emph')
                         + " %s\n" % (process,)
                         + set_style("stdout:", 'emph') + "\n%s\n" % (process.stdout())
                         + set_style("stderr:", 'emph') + "\n%s\n" % (process.stderr()))
            if (process.ok()):
                newly_deployed.append(process.host())
                logger.info("OK %s", process.host())
            else:
                logger.info("KO %s", process.host())
        return newly_deployed

    start_time = time.time()
    deployed_hosts = set()
    undeployed_hosts = get_hosts_set(deployment.hosts)
    my_newly_deployed = None
    if check_deployed_command:
        my_newly_deployed = check_update_deployed(deployed_hosts, undeployed_hosts, check_deployed_command, node_connexion_params)
        deployed_hosts.update(my_newly_deployed)
        undeployed_hosts.difference_update(my_newly_deployed)
    num_tries_done = 0
    elapsed = time.time() - start_time
    last_time = time.time()
    deploy_stats = list()   # contains tuples ( timestamp,
                            #                   num attempted deploys,
                            #                   len(kadeploy_newly_deployed),
                            #                   len(my_newly_deployed),
                            #                   len(deployed_hosts),
                            #                   len(undeployed_hosts )
    deploy_stats.append((elapsed, None, None, len(my_newly_deployed), len(deployed_hosts), len(undeployed_hosts)))
    while (not check_enough_func(deployed_hosts, undeployed_hosts)
           and num_tries_done < num_tries):
        num_tries_done += 1
        logger.info(set_style("try %i, deploying on:" % (num_tries_done,), 'emph') + " %s", undeployed_hosts)
        tmp_deployment = copy.copy(deployment)
        tmp_deployment.hosts = undeployed_hosts
        (kadeploy_newly_deployed, _) = kadeploy(tmp_deployment,
                                                out = out,
                                                frontend_connexion_params = frontend_connexion_params,
                                                timeout = deploy_timeout)
        my_newly_deployed = None
        if check_deployed_command:
            my_newly_deployed = check_update_deployed(deployed_hosts, undeployed_hosts, check_deployed_command, node_connexion_params)
            deployed_hosts.update(my_newly_deployed)
            undeployed_hosts.difference_update(my_newly_deployed)
        else:
            deployed_hosts.update(kadeploy_newly_deployed)
            undeployed_hosts.difference_update(kadeploy_newly_deployed)
        logger.info(set_style("kadeploy reported newly deployed hosts:", 'emph') + "   %s", kadeploy_newly_deployed)
        logger.info(set_style("check reported newly deployed hosts:", 'emph') + "   %s", my_newly_deployed)
        logger.info(set_style("all deployed hosts:", 'emph') + "     %s", deployed_hosts)
        logger.info(set_style("still undeployed hosts:", 'emph') + " %s", undeployed_hosts)
        elapsed = time.time() - last_time
        last_time = time.time()
        deploy_stats.append((elapsed,
                             len(tmp_deployment.hosts),
                             len(kadeploy_newly_deployed),
                             len(my_newly_deployed),
                             len(deployed_hosts),
                             len(undeployed_hosts)))

    logger.info(set_style("deploy finished", 'emph') + " in %i tries, %s", num_tries_done, format_seconds(time.time() - start_time))
    logger.info("deploy  duration  attempted  deployed     deployed     total     total")
    logger.info("                  deploys    as reported  as reported  already   still")
    logger.info("                             by kadeploy  by check     deployed  undeployed")
    logger.info("---------------------------------------------------------------------------")
    for (deploy_index, deploy_stat) in enumerate(deploy_stats):
        logger.info("#%-5.5s  %-8.8s  %-9.9s  %-11.11s  %-11.11s  %-8.8s  %-10.10s",
            deploy_index,
            format_seconds(deploy_stat[0]),
            deploy_stat[1],
            deploy_stat[2],
            deploy_stat[3],
            deploy_stat[4],
            deploy_stat[5])
    logger.info(set_style("deployed hosts:", 'emph') + " %s", deployed_hosts)
    logger.info(set_style("undeployed hosts:", 'emph') + " %s", undeployed_hosts)

    return (deployed_hosts, undeployed_hosts)
