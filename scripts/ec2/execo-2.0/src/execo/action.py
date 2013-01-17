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

from execo.config import make_connexion_params, configuration, SSH, TAKTUK, SCP
from execo.host import Host
from execo.process import get_process
from host import get_hosts_list
from log import set_style, logger
from process import ProcessLifecycleHandler, SshProcess, ProcessOutputHandler, \
    TaktukProcess, Process
from report import Report
from ssh_utils import get_rewritten_host_address, get_scp_command, \
    get_taktuk_connector_command, get_ssh_command
from substitutions import get_caller_context, remote_substitute
from time_utils import get_seconds, format_date
import threading
import time

class ActionLifecycleHandler(object):

    """Abstract handler for `execo.action.Action` lifecycle."""

    def start(self, action):
        """Handle `execo.action.Action`'s start.

        :param action: The Action which starts.
        """
        pass

    def end(self, action):
        """Handle `execo.action.Action`'s end.

        :param action: The Action which ends.
        """
        pass

    def reset(self, action):
        """Handle `execo.action.Action`'s reset.

        :param action: the Action which is reset.
        """
        pass

class Action(object):

    """Abstract base class. A set of parallel processes.

    An `execo.action.Action` can be started
    (`execo.action.Action.start`), killed
    (`execo.action.Action.kill`). One can wait
    (`execo.action.Action.wait`) for an Action, it means waiting for
    all processes in the Action to finish. An Action can be run
    (`execo.action.Action.run`), it means start it then wait for it to
    complete.
    """

    _wait_multiple_actions_condition = threading.Condition()

    def __init__(self, name = None):
        """
        :param name: Action name, one will be generated if None given
        """
        self._end_event = threading.Event()
        self._started = False
        self._ended = False
        self._end_event.clear()
        self._name = name
        self._lifecycle_handler = list()

    def _common_reset(self):
        # all methods _common_reset() of this class hierarchy contain
        # the code common to the constructor and reset() to
        # reinitialize an object. _common_reset() of child classes
        # must explicitely call _common_reset() of their parent class.
        self._started = False
        self._ended = False
        self._end_event.clear()

    def _args(self):
        # to be implemented in all subclasses. Must return a list with
        # all arguments to the constructor, beginning by the
        # positionnal arguments, finishing by keyword arguments. This
        # list will be directly used in __repr__ methods.
        return Action._kwargs(self)

    def _kwargs(self):
        # to be implemented in all subclasses. Must return a list with
        # all keyword arguments to the constructor. This list will be
        # used to build the list returned by _args() of this class or
        # child classes.
        kwargs = []
        if self._name: kwargs.append("name=%r" % (self._name,))
        return kwargs

    def _infos(self):
        # to be implemented in all subclasses. Must return a list with
        # all relevant infos other than those returned by _args(), for
        # use in __str__ methods.
        stats = self.stats()
        return [ "started=%r" % (self._started,),
                 "start_date=%r" % (format_date(stats['start_date']),),
                 "ended=%r" % (self._ended,),
                 "end_date=%r" % (format_date(stats['end_date']),),
                 "num_processes=%r" % (stats['num_processes'],),
                 "num_started=%r" % (stats['num_started'],),
                 "num_ended=%r" % (stats['num_ended'],),
                 "num_timeouts=%r" % (stats['num_timeouts'],),
                 "num_errors=%r" % (stats['num_errors'],),
                 "num_forced_kills=%r" % (stats['num_forced_kills'],),
                 "num_non_zero_exit_codes=%r" % (stats['num_non_zero_exit_codes'],),
                 "num_ok=%r" % (stats['num_ok'],),
                 "num_finished_ok=%r" % (stats['num_finished_ok'],),
                 "ok=%r" % (self.ok(),) ]

    def __repr__(self):
        # implemented once for all subclasses
        return "%s(%s)" % (self.__class__.__name__, ", ".join(self._args()))

    def __str__(self):
        # implemented once for all subclasses
        return "<" + set_style(self.__class__.__name__, 'object_repr') + "(%s)>" % (", ".join(self._args() + self._infos()),)

    def name(self):
        """Return the `execo.report.Report` name."""
        if self._name == None:
            return "%s 0x%08.8x" % (self.__class__.__name__, id(self))
        else:
            return self._name

    def add_lifecycle_handler(self, handler):
        """Add a lifecycle handler.

        An instance of `execo.action.ActionLifecycleHandler` for being
        notified of action lifecycle events.
        """
        self._lifecycle_handler.append(handler)

    def _notify_terminated(self):
        with Action._wait_multiple_actions_condition:
            logger.debug(set_style("got termination notification for:", 'emph') + " %s", self)
            for handler in self._lifecycle_handler:
                handler.end(self)
            self._ended = True
            self._end_event.set()
            Action._wait_multiple_actions_condition.notifyAll()

    def start(self):
        """Start all processes.

        return self"""
        if self._started:
            raise ValueError, "Actions may be started only once"
        self._started = True
        logger.debug(set_style("start:", 'emph') + " %s", self)
        for handler in self._lifecycle_handler:
            handler.start(self)
        return self

    def kill(self):
        """Kill all processes not yet ended.

        Returns immediately, without waiting for processes to be
        actually killed.

        return self"""
        logger.debug(set_style("kill:", 'emph') + " %s", self)
        return self
    
    def wait(self, timeout = None):
        """Wait for all processes to complete.

        return self"""
        logger.debug(set_style("start waiting:", 'emph') + " %s", self)
        self._end_event.wait(get_seconds(timeout))
        logger.debug(set_style("end waiting:", 'emph') + " %s", self)
        return self

    def run(self, timeout = None):
        """Start all processes then wait for them to complete.

        return self"""
        logger.debug(set_style("run:", 'emph') + " %s", self)
        self.start()
        self.wait(timeout)
        return self

    def reset(self):
        """Reinitialize an Action so that it can later be restarted.

        If it is running, this method will first kill it then wait for
        its termination before reseting.
        """
        logger.debug(set_style("reset:", 'emph') + " %s", self)
        if self._started and not self._ended:
            self.kill()
            self.wait()
        for handler in self._lifecycle_handler:
            handler.reset(self)
        self._common_reset()
        return self

    def processes(self):
        """Return an iterable of all `execo.process.ProcessBase`."""
        return ()

    def started(self):
        """Return whether this Action was started (boolean)."""
        return self._started

    def ended(self):
        """Return whether all processes of this Action have ended (boolean)."""
        return self._ended

    def ok(self):
        """Returns a boolean indicating if all processes are ok.

        refer to `execo.process.ProcessBase.ok` for detailed semantics
        of being ok for a process.
        """
        return all([process.ok() for process in self.processes()])

    def finished_ok(self):
        """Action has started, ended, and is not in error."""
        return self.started() and self.ended() and self.ok()

    def stats(self):
        """Return a dict summarizing the statistics of all processes
        of this Action.

        see `execo.report.Report.stats`.
        """
        stats = Report.empty_stats()
        stats['name'] = self.name()
        for process in self.processes():
            if (stats['start_date'] == None
                or (process.start_date() != None
                    and process.start_date() < stats['start_date'])):
                stats['start_date'] = process.start_date()
            if (stats['end_date'] == None
                or (process.end_date() != None
                    and process.end_date() > stats['end_date'])):
                stats['end_date'] = process.end_date()
            stats['num_processes'] += 1
            if process.started(): stats['num_started'] += 1
            if process.ended(): stats['num_ended'] += 1
            if process.error(): stats['num_errors'] += 1
            if process.timeouted(): stats['num_timeouts'] += 1
            if process.forced_kill(): stats['num_forced_kills'] += 1
            if (process.started()
                and process.ended()
                and process.exit_code() != 0):
                stats['num_non_zero_exit_codes'] += 1
            if process.ok():
                stats['num_ok'] += 1
            if process.finished_ok():
                stats['num_finished_ok'] +=1
        if stats['num_processes'] > stats['num_ended']:
            stats['end_date'] = None
        return stats

def wait_multiple_actions(actions, timeout = None):
    """Wait for any of the actions given to terminate.

    :param actions: An iterable of `execo.action.Action`.

    :param timeout: Optional timeout in any type supported by
      `execo.time_utils.get_seconds`.

    returns: iterable of `execo.action.Action` which have terminated.
    """
    timeout = get_seconds(timeout)
    if timeout != None:
        end = time.time() + timeout
    with Action._wait_multiple_actions_condition:
        finished = [action for action in actions if action.ended()]
        while len(finished) == 0 and (timeout == None or timeout > 0):
            Action._wait_multiple_actions_condition.wait(get_seconds(timeout))
            finished = [action for action in actions if action.ended()]
            if timeout != None:
                timeout = end - time.time()
        return finished

def wait_all_actions(actions, timeout = None):
    """Wait for all of the actions given to terminate.

    :param actions: An iterable of `execo.action.Action`.

    :param timeout: Optional timeout in any type supported by
      `execo.time_utils.get_seconds`.

    returns: iterable of `execo.action.Action` which have terminated.
    """
    timeout = get_seconds(timeout)
    if timeout != None:
        end = time.time() + timeout
    with Action._wait_multiple_actions_condition:
        finished = [action for action in actions if action.ended()]
        while len(finished) != len(actions) and (timeout == None or timeout > 0):
            Action._wait_multiple_actions_condition.wait(get_seconds(timeout))
            finished = [action for action in actions if action.ended()]
            if timeout != None:
                timeout = end - time.time()
        return finished

class ActionNotificationProcessLifecycleHandler(ProcessLifecycleHandler):

    def __init__(self, action, total_processes):
        super(ActionNotificationProcessLifecycleHandler, self).__init__()
        self._action = action
        self._total_processes = total_processes
        self._terminated_processes = 0

    def end(self, process):
        self._terminated_processes += 1
        logger.debug("%i/%i processes terminated in %s",
            self._terminated_processes,
            self._total_processes,
            self._action)
        if self._terminated_processes == self._total_processes:
            self._action._notify_terminated()

    def action_reset(self):
        self._terminated_processes = 0

class Remote(Action):

    """Launch a command remotely on several host, with ``ssh`` or a similar remote connexion tool.

    One ssh process is launched for each connexion.
    """

    def __init__(self, cmd, hosts, connexion_params = None, name = None, **kwargs):
        """
        :param cmd: the command to run remotely. substitions
          described in `execo.substitutions.remote_substitute` will be
          performed.

        :param hosts: iterable of `execo.host.Host` to which to
          connect and run the command.

        :param connexion_params: a dict similar to
          `execo.config.default_connexion_params` whose values will
          override those in default_connexion_params for connexion.

        :param name: action's name

        :param kwargs: passed to the instanciated
          `execo.process.SshProcess`.
        """
        super(Remote, self).__init__(name = name)
        self._cmd = cmd
        self._connexion_params = connexion_params
        self._other_kwargs = kwargs
        self._caller_context = get_caller_context()
        self._hosts = get_hosts_list(hosts)
        self._processes = list()
        self._process_lifecycle_handler = ActionNotificationProcessLifecycleHandler(self, len(self._hosts))
        for (index, host) in enumerate(self._hosts):
            p = SshProcess(remote_substitute(cmd, self._hosts, index, self._caller_context),
                           host = host,
                           connexion_params = connexion_params,
                           process_lifecycle_handler = self._process_lifecycle_handler,
                           **self._other_kwargs)
            self._processes.append(p)

    def _args(self):
        return [ repr(self._hosts),
                 repr(self._cmd) ] + Action._args(self) + Remote._kwargs(self)

    def _kwargs(self):
        kwargs = []
        if self._connexion_params: kwargs.append("connexion_params=%r" % (self._connexion_params,))
        for (k, v) in self._other_kwargs.iteritems(): kwargs.append("%s=%s" % (k, v))
        return kwargs

    def _infos(self):
        return Action._infos(self)

    def name(self):
        if self._name == None:
            return "%s on %i hosts: %s" % (self.__class__.__name__, len(self._hosts), self._cmd)
        else:
            return self._name

    def processes(self):
        return list(self._processes)

    def start(self):
        retval = super(Remote, self).start()
        if len(self._processes) == 0:
            logger.debug("%s contains 0 processes -> immediately terminated", self)
            self._notify_terminated()
        else:
            for process in self._processes:
                process.start()
        return retval

    def kill(self):
        retval = super(Remote, self).kill()
        for process in self._processes:
            if process.running():
                process.kill()
        return retval

    def reset(self):
        retval = super(Remote, self).reset()
        for process in self._processes:
            process.reset()
        self._process_lifecycle_handler.action_reset()
        return retval

class _TaktukRemoteOutputHandler(ProcessOutputHandler):

    """Parse taktuk output."""
    
    def __init__(self, taktukaction):
        super(_TaktukRemoteOutputHandler, self).__init__()
        self._taktukaction = taktukaction

    def _log_unexpected_output(self, string):
        logger.critical("%s: Taktuk unexpected output parsing. Please report this message. Line received:", self.__class__.__name__)
        logger.critical(self._describe_taktuk_output(string))

    def _describe_taktuk_output(self, string):
        s = "%s: " % (self.__class__.__name__,)
        try:
            if len(string) > 0:
                header = ord(string[0])
                (position, _, line) = string[2:].partition(" # ")
                position = int(position)
                if position == 0:
                    host_address = "localhost"
                else:
                    host_address = self._taktukaction._processes[self._taktukaction._taktuk_hosts_order[position-1]].host().address
                if header in (65, 66, 67, 70, 71, 72):
                    if header == 65: t = "stdout"
                    elif header == 66: t = "stderr"
                    elif header == 67: t = "status"
                    elif header == 70: t = "info"
                    elif header == 71: t = "taktuk"
                    elif header == 72: t = "message"
                    s += "%s - host = %s - line = %s" % (t, host_address, line[:-1])
                elif header in (68, 69):
                    (peer_position, _, line) = line.partition(" # ")
                    peer_host_address = None
                    try:
                        peer_position = int(peer_position)
                        if peer_position == 0:
                            peer_host_address = "localhost"
                        else:
                            peer_host_address = self._taktukaction._processes[self._taktukaction._taktuk_hosts_order[peer_position-1]].host().address
                    except ValueError:
                        pass
                    if header == 68:
                        s += "connector - host = %s - peer = %s - line = %s" % (host_address, peer_host_address, line[:-1])
                    elif header == 69:
                        (_, _, state_msg) = line.partition(" # ")
                        s += "state - host = %s - peer = %s - state = %s" % (host_address, peer_host_address, state_msg[:-1])
                elif header == 73:
                    (taktuktype, _, line) = line.partition(" # ")
                    s += "message type = %s - host = %s - line = %s" % (taktuktype, host_address, line[:-1])
                else:
                    s += "unexpected string = %s" % (string[:-1])
            else:
                s += "empty string"
            return s
        except Exception, e: #IGNORE:W0703
            logger.critical("%s: Unexpected exception %s while parsing taktuk output. Please report this message.", self.__class__.__name__, e)
            logger.critical("line received = %s", string.rstrip('\n'))
            return s

    def read_line(self, process, string, eof = False, error = False):
        # my taktuk output protocol:
        #  stream    format                                                    header normal?
        #  output    "A $position # $line"                                     65     YES
        #  error     "B $position # $line"                                     66     YES
        #  status    "C $position # $line"                                     67     YES
        #  connector "D $position # $peer_position # $line"                    68     YES
        #  state     "E $position # $peer_position # $line # event_msg($line)" 69     YES
        #  info      "F $position # $line"                                     70     NO
        #  taktuk    "G $position # $line"                                     71     NO
        #  message   "H $position # $line"                                     72     NO
        #  default   "I $position # $type # $line"                             73     NO
        try:
            if len(string) > 0:
                #logger.debug("taktuk: %s", self._describe_taktuk_output(string))
                header = ord(string[0])
                (position, _, line) = string[2:].partition(" # ")
                position = int(position)
                if header >= 65 and header <= 67: # stdout, stderr, status
                    if position == 0:
                        self._log_unexpected_output(string)
                        return
                    else:
                        process = self._taktukaction._processes[self._taktukaction._taktuk_hosts_order[position-1]]
                        if header == 65: # stdout
                            process._handle_stdout(line, eof = eof, error = error)
                        elif header == 66: # stderr
                            process._handle_stderr(line, eof = eof, error = error)
                        else: # 67: status
                            process._set_terminated(exit_code = int(line))
                elif header in (68, 69): # connector, state
                    (peer_position, _, line) = line.partition(" # ")
                    if header == 68: # connector
                        peer_position = int(peer_position)
                        process = self._taktukaction._processes[self._taktukaction._taktuk_hosts_order[peer_position-1]]
                        process._handle_stderr(line)
                    else: # state
                        (state_code, _, _) = line.partition(" # ")
                        state_code = int(state_code)
                        if state_code == 6 or state_code == 7: # command started or remote command exec failed
                            process = self._taktukaction._processes[self._taktukaction._taktuk_hosts_order[position-1]]
                            if state_code == 6: # command started
                                process.start()
                            else: # 7: remote command exec failed
                                process._set_terminated(error = True, error_reason = "taktuk remote command execution failed")
                        elif state_code == 3 or state_code == 5: # connexion failed or lost
                            peer_position = int(peer_position)
                            process = self._taktukaction._processes[self._taktukaction._taktuk_hosts_order[peer_position-1]]
                            if state_code == 3: # connexion failed
                                process._set_terminated(error = True, error_reason = "taktuk connexion failed")
                            else: # 5: connexion lost
                                process._set_terminated(error = True, error_reason = "taktuk connexion lost")
                        elif state_code in (0, 1, 2, 4, 8):
                            pass
                        else:
                            self._log_unexpected_output(string)
                else:
                    self._log_unexpected_output(string)
        except Exception, e: #IGNORE:W0703
            logger.critical("%s: Unexpected exception %s while parsing taktuk output. Please report this message.", self.__class__.__name__, e)
            logger.critical("line received = %s", string.rstrip('\n'))

class _TaktukLifecycleHandler(ProcessLifecycleHandler):

    """Notify `execo.process.TaktukProcess` of their real taktuk process lifecyle."""
    
    def __init__(self, taktukremote):
        super(_TaktukLifecycleHandler, self).__init__()
        self._taktukremote = taktukremote

    def end(self, process):
        error = None
        error_reason = None
        timeouted = None
        forced_kill = None
        if process.error() == True:
            error = True
        if process.error_reason() != None:
            error_reason = "error of taktuk process: %s" % process.error_reason()
        if process.timeouted() == True:
            timeouted = True
        if process.forced_kill() == True:
            forced_kill = True
        for taktukprocess in self._taktukremote.processes():
            if not taktukprocess.ended():
                taktukprocess._set_terminated(error = error,
                                              error_reason = error_reason,
                                              timeouted = timeouted,
                                              forced_kill = forced_kill)

class TaktukRemote(Action):

    """Launch a command remotely on several host, with ``taktuk``.

    One taktuk instance is ran, which itself connects to hosts through
    an ``ssh`` tree.

    Behavior should be identical to `execo.action.Remote`. Current
    limitation are:

    - we can provide per-host user with taktuk, but we cannot provide
      per-host port or keyfile, so a check is made that all hosts and
      connexion_params have the same port / keyfile (or None). If not,
      an exception is raised during initialization.

    - remote processes are not killed when killing the
      TaktukRemote. See 'hanged commands' in
      http://taktuk.gforge.inria.fr/taktuk.html#bugs. With ssh the
      workaround is to pass options -tt but passing these options to
      taktuk connector causes immediate closing of the connector upon
      connexion.
    """

    def __init__(self, cmd, hosts, connexion_params = None, name = None, **kwargs):
        """
        :param hosts: iterable of `execo.host.Host` to which to
          connect and run the command.

        :param cmd: the command to run remotely. substitions
          described in `execo.substitutions.remote_substitute` will be
          performed.

        :param connexion_params: a dict similar to
          `execo.config.default_connexion_params` whose values will
          override those in default_connexion_params for connexion.

        :param name: action's name

        :param kwargs: passed to the instanciated
          `execo.process.TaktukProcess`. Special case for the
          ``timeout`` keyword argument: it will be passed to the dummy
          `execo.process.TaktukProcess`, but will also be used as
          timeout for the underlying real taktuk process.
        """
        super(TaktukRemote, self).__init__(name = name)
        self._cmd = cmd
        self._connexion_params = connexion_params
        self._other_kwargs = kwargs
        self._caller_context = get_caller_context()
        self._hosts = get_hosts_list(hosts)
        self._processes = list()
        self._taktuk_stdout_output_handler = _TaktukRemoteOutputHandler(self)
        self._taktuk_stderr_output_handler = self._taktuk_stdout_output_handler
        self._taktuk_hosts_order = []
        self._taktuk_cmdline = ()
        self._taktuk = None
        self._taktuk_common_init()

    def _args(self):
        return [ repr(self._hosts),
                 repr(self._cmd) ] + Action._args(self) + TaktukRemote._kwargs(self)

    def _kwargs(self):
        kwargs = []
        if self._connexion_params: kwargs.append("connexion_params=%r" % (self._connexion_params,))
        for (k, v) in self._other_kwargs.iteritems(): kwargs.append("%s=%s" % (k, v))
        return kwargs

    def _infos(self):
        return Action._infos(self)

    def _gen_taktukprocesses(self):
        lifecycle_handler = ActionNotificationProcessLifecycleHandler(self, len(self._hosts))
        for (index, host) in enumerate(self._hosts):
            p = TaktukProcess(remote_substitute(self._cmd, self._hosts, index, self._caller_context),
                              host = host,
                              process_lifecycle_handler = lifecycle_handler,
                              **self._other_kwargs)
            self._processes.append(p)

    def _gen_taktuk_commands(self, hosts_with_explicit_user):
        for (index, host) in [ (idx, h) for (idx, h) in enumerate(self._hosts) if h not in hosts_with_explicit_user ]:
            self._taktuk_cmdline += ("-m", get_rewritten_host_address(host.address, self._connexion_params), "-[", "exec", "[", self._processes[index].cmd(), "]", "-]",)
            self._taktuk_hosts_order.append(index)
        for (index, host) in [ (idx, h) for (idx, h) in enumerate(self._hosts) if h in hosts_with_explicit_user ]:
            self._taktuk_cmdline += ("-l", host.user, "-m", get_rewritten_host_address(host.address, self._connexion_params), "-[", "exec", "[", self._processes[index].cmd(), "]", "-]",)
            self._taktuk_hosts_order.append(index)

    def _taktuk_common_init(self):
        # taktuk code common to TaktukRemote and subclasses TaktukGet
        # TaktukPut

        # we can provide per-host user with taktuk, but we cannot
        # provide per-host port or keyfile, so check that all hosts
        # and connexion_params have the same port / keyfile (or None)
        actual_connexion_params = make_connexion_params(self._connexion_params)
        check_default_port = actual_connexion_params['port']
        check_default_keyfile = actual_connexion_params['keyfile']
        check_keyfiles = set()
        check_ports = set()
        hosts_with_explicit_user = set()
        for host in self._hosts:
            if host.user != None:
                hosts_with_explicit_user.add(host)
            if host.keyfile != None:
                check_keyfiles.add(host.keyfile)
            else:
                check_keyfiles.add(check_default_keyfile)
            if host.port != None:
                check_ports.add(host.port)
            else:
                check_ports.add(check_default_port)
        if len(check_keyfiles) > 1 or len(check_ports) > 1:
            raise ValueError, "unable to provide more than one keyfile / port for taktuk remote connexion"
        global_keyfile = None
        global_port = None
        if len(check_keyfiles) == 1:
            global_keyfile = list(check_keyfiles)[0]
        if len(check_ports) == 1:
            global_port = list(check_ports)[0]
        self._gen_taktukprocesses()
        self._taktuk_cmdline += (actual_connexion_params['taktuk'],)
        self._taktuk_cmdline += actual_connexion_params['taktuk_options']
        self._taktuk_cmdline += ("-o", 'output="A $position # $line\\n"',
                                 "-o", 'error="B $position # $line\\n"',
                                 "-o", 'status="C $position # $line\\n"',
                                 "-o", 'connector="D $position # $peer_position # $line\\n"',
                                 "-o", 'state="E $position # $peer_position # $line # ".event_msg($line)."\\n"',
                                 "-o", 'info="F $position # $line\\n"',
                                 "-o", 'taktuk="G $position # $line\\n"',
                                 "-o", 'message="H $position # $line\\n"',
                                 "-o", 'default="I $position # $type > $line\\n"')
        self._taktuk_cmdline += ("-c", " ".join(
            get_taktuk_connector_command(keyfile = global_keyfile,
                                         port = global_port,
                                         connexion_params = self._connexion_params)))
        self._gen_taktuk_commands(hosts_with_explicit_user)
        self._taktuk_cmdline += ("quit",)
        #handler = _TaktukRemoteOutputHandler(self)
        taktuk_gateway = None
        if actual_connexion_params.get('taktuk_gateway'):
            taktuk_gateway = Host(actual_connexion_params['taktuk_gateway'])
        self._taktuk = get_process(self._taktuk_cmdline,
                                   host = taktuk_gateway,
                                   connexion_params = actual_connexion_params['taktuk_gateway_connexion_params'],
                                   timeout = self._kwargs.get('timeout'),
                                   stdout_handler = self._taktuk_stdout_output_handler,
                                   stderr_handler = self._taktuk_stderr_output_handler,
                                   #default_stdout_handler = False,
                                   #default_stderr_handler = False,
                                   process_lifecycle_handler = _TaktukLifecycleHandler(self))

    def processes(self):
        return list(self._processes)

    def start(self):
        retval = super(TaktukRemote, self).start()
        if len(self._processes) == 0:
            logger.debug("%s contains 0 processes -> immediately terminated", self)
            self._notify_terminated()
        else:
            self._taktuk.start()
        return retval

    def kill(self):
        retval = super(TaktukRemote, self).kill()
        self._taktuk.kill()
        return retval

    def reset(self):
        retval = super(TaktukRemote, self).reset()
        self._taktuk.reset()
        for process in self._processes:
            process.reset()
        return retval

class Put(Remote):

    """Copy local files to several remote host, with ``scp`` or a similar connexion tool."""

    def __init__(self, hosts, local_files, remote_location = ".", create_dirs = False, connexion_params = None, name = None, **kwargs):
        """
        :param hosts: iterable of `execo.host.Host` onto which to copy
          the files.

        :param local_files: an iterable of string of file
          paths. substitions described in
          `execo.substitutions.remote_substitute` will be performed.
        
        :param remote_location: the directory on the remote hosts were
          the files will be copied. substitions described in
          `execo.substitutions.remote_substitute` will be performed.

        :param create_dirs: boolean indicating if remote_location is a
          directory to be created

        :param connexion_params: a dict similar to
          `execo.config.default_connexion_params` whose values will
          override those in default_connexion_params for connexion.

        :param name: action's name

        :param kwargs: passed to the instanciated processes. Special
          case: do not use the shell keyword argument, as it is used
          internally.
        """
        if local_files != None and (not hasattr(local_files, '__iter__')):
            local_files = (local_files,)
        super(Remote, self).__init__(name = name)
        self._caller_context = get_caller_context()
        self._hosts = get_hosts_list(hosts)
        self._processes = list()
        self._local_files = local_files
        self._remote_location = remote_location
        self._create_dirs = create_dirs
        self._connexion_params = connexion_params
        self._other_kwargs = kwargs
        lifecycle_handler = ActionNotificationProcessLifecycleHandler(self, len(self._hosts))
        for (index, host) in enumerate(self._hosts):
            prepend_dir_creation = ()
            if self._create_dirs:
                created_dir = remote_substitute(self._remote_location, self._hosts, index, self._caller_context)
                prepend_dir_creation = get_ssh_command(host.user, host.keyfile, host.port, self._connexion_params) + (get_rewritten_host_address(host.address, self._connexion_params),) + ('\'mkdir -p "%(dir)s" || test -d "%(dir)s"\'' % {'dir': created_dir}, '&&')
            real_command = list(prepend_dir_creation) + list(get_scp_command(host.user, host.keyfile, host.port, self._connexion_params)) + [ remote_substitute(local_file, self._hosts, index, self._caller_context) for local_file in self._local_files ] + ["%s:%s" % (get_rewritten_host_address(host.address, self._connexion_params), remote_substitute(self._remote_location, self._hosts, index, self._caller_context)),]
            real_command = ' '.join(real_command)
            p = Process(real_command,
                        shell = True,
                        process_lifecycle_handler = lifecycle_handler,
                        **self._other_kwargs)
            self._processes.append(p)

    def _args(self):
        return [ repr(self._hosts),
                 repr(self._local_files) ] + Action._args(self) + Put._kwargs(self)

    def _kwargs(self):
        kwargs = []
        if self._remote_location: kwargs.append("remote_location=%r" % (self._remote_location,))
        if self._create_dirs: kwargs.append("create_dirs=%r" % (self._create_dirs,))
        if self._connexion_params: kwargs.append("connexion_params=%r" % (self._connexion_params,))
        for (k, v) in self._other_kwargs.iteritems(): kwargs.append("%s=%s" % (k, v))
        return kwargs

    def _infos(self):
        return Action._infos(self)

    def name(self):
        if self._name == None:
            return "%s to %i hosts" % (self.__class__.__name__, len(self._hosts))
        else:
            return self._name

class Get(Remote):

    """Copy remote files from several remote host to a local directory, with ``scp`` or a similar connexion tool."""

    def __init__(self, hosts, remote_files, local_location = ".", create_dirs = False, connexion_params = None, name = None, **kwargs):
        """
        :param hosts: iterable of `execo.host.Host` from which to get
          the files.

        :param remote_files: an iterable of string of file
          paths. substitions described in
          `execo.substitutions.remote_substitute` will be performed.

        :param local_location: the local directory were the files will
          be copied. substitions described in
          `execo.substitutions.remote_substitute` will be performed.

        :param create_dirs: boolean indicating if local_location is a
          directory to be created

        :param connexion_params: a dict similar to
          `execo.config.default_connexion_params` whose values will
          override those in default_connexion_params for connexion.

        :param name: action's name

        :param kwargs: passed to the instanciated processes. Special
          case: do not use the shell keyword argument, as it is used
          internally.
        """
        if remote_files != None and (not hasattr(remote_files, '__iter__')):
            remote_files = (remote_files,)
        super(Remote, self).__init__(name = name)
        self._caller_context = get_caller_context()
        self._hosts = get_hosts_list(hosts)
        self._processes = list()
        self._remote_files = remote_files
        self._local_location = local_location
        self._create_dirs = create_dirs
        self._connexion_params = connexion_params
        self._other_kwargs = kwargs
        lifecycle_handler = ActionNotificationProcessLifecycleHandler(self, len(self._hosts))
        for (index, host) in enumerate(self._hosts):
            prepend_dir_creation = ()
            if self._create_dirs:
                created_dir = remote_substitute(local_location, self._hosts, index, self._caller_context)
                prepend_dir_creation = ('mkdir', '-p', created_dir, '||', 'test', '-d', created_dir, '&&')
            remote_specs = ()
            for path in self._remote_files:
                remote_specs += ("%s:%s" % (get_rewritten_host_address(host.address, self._connexion_params), remote_substitute(path, self._hosts, index, self._caller_context)),)
            real_command = prepend_dir_creation + get_scp_command(host.user, host.keyfile, host.port, self._connexion_params) + remote_specs + (remote_substitute(self._local_location, self._hosts, index, self._caller_context),)
            real_command = ' '.join(real_command)
            p = Process(real_command,
                        shell = True,
                        process_lifecycle_handler = lifecycle_handler,
                        **self._other_kwargs)
            self._processes.append(p)

    def _args(self):
        return [ repr(self._hosts),
                 repr(self._remote_files) ] + Action._args(self) + Get._kwargs(self)

    def _kwargs(self):
        kwargs = []
        if self._local_location: kwargs.append("local_location=%r" % (self._local_location,))
        if self._create_dirs: kwargs.append("create_dirs=%r" % (self._create_dirs,))
        if self._connexion_params: kwargs.append("connexion_params=%r" % (self._connexion_params,))
        for (k, v) in self._other_kwargs.iteritems(): kwargs.append("%s=%s" % (k, v))
        return kwargs

    def _infos(self):
        return Action._infos(self)

    def name(self):
        if self._name == None:
            return "%s from %i hosts" % (self.__class__.__name__, len(self._hosts))
        else:
            return self._name

class _TaktukPutOutputHandler(_TaktukRemoteOutputHandler):

    """Parse taktuk output."""

    def _update_taktukprocess_end_state(self, process):
        if process._num_transfers_started > 0 and not process.started():
            process.start()
        if process._num_transfers_failed + process._num_transfers_terminated >= len(self._taktukaction._local_files):
            if process._num_transfers_failed > 0:
                process._set_terminated(error = True, error_reason = "taktuk file reception failed")
            else:
                process._set_terminated(exit_code = 0)
    
    def read_line(self, process, string, eof = False, error = False):
        try:
            if len(string) > 0:
                header = ord(string[0])
                (position, _, line) = string[2:].partition(" # ")
                position = int(position)
                if header in (68, 69): # connector, state
                    (peer_position, _, line) = line.partition(" # ")
                    if header == 68: # connector
                        peer_position = int(peer_position)
                        process = self._taktukaction._processes[self._taktukaction._taktuk_hosts_order[peer_position-1]]
                        process._handle_stderr(line)
                    else: # state
                        (state_code, _, _) = line.partition(" # ")
                        state_code = int(state_code)
                        if state_code in (13, 14, 15): # file reception started, failed, terminated
                            process = self._taktukaction._processes[self._taktukaction._taktuk_hosts_order[position-1]]
                            if state_code == 13: # file reception started
                                process._num_transfers_started += 1
                            elif state_code == 14: # file reception failed
                                process._num_transfers_failed += 1
                            else: # 15: file reception terminated
                                process._num_transfers_terminated += 1
                            self._update_taktukprocess_end_state(process)
                        elif state_code == 3 or state_code == 5: # connexion failed or lost
                            peer_position = int(peer_position)
                            process = self._taktukaction._processes[self._taktukaction._taktuk_hosts_order[peer_position-1]]
                            if state_code == 3: # connexion failed
                                process._set_terminated(error = True, error_reason = "taktuk connexion failed")
                            else: # 5: connexion lost
                                process._set_terminated(error = True, error_reason = "taktuk connexion lost")
                        elif state_code in (0, 1, 2, 4):
                            pass
                        else:
                            self._log_unexpected_output(string)
                else:
                    self._log_unexpected_output(string)
        except Exception, e: #IGNORE:W0703
            logger.critical("%s: Unexpected exception %s while parsing taktuk output. Please report this message.", self.__class__.__name__, e)
            logger.critical("line received = %s", string.rstrip('\n'))

class TaktukPut(TaktukRemote):

    """Copy local files to several remote host, with ``taktuk``."""

    def __init__(self, hosts, local_files, remote_location = ".", connexion_params = None, name = None, **kwargs):
        """
        :param hosts: iterable of `execo.host.Host` onto which to copy
          the files.

        :param local_files: an iterable of string of file
          paths. substitions described in
          `execo.substitutions.remote_substitute` will not be
          performed, but taktuk substitutions can be used (see
          http://taktuk.gforge.inria.fr/taktuk.html#item_put__2a_src__2a__2a_dest__2a)
        
        :param remote_location: the directory on the remote hosts were
          the files will be copied. substitions described in
          `execo.substitutions.remote_substitute` will not be
          performed, but taktuk substitutions can be used (see
          http://taktuk.gforge.inria.fr/taktuk.html#item_put__2a_src__2a__2a_dest__2a)

        :param connexion_params: a dict similar to
          `execo.config.default_connexion_params` whose values will
          override those in default_connexion_params for connexion.

        :param name: action's name

        :param kwargs: passed to the instanciated
          `execo.process.TaktukProcess`. Special case for the
          ``timeout`` keyword argument: it will be passed to the dummy
          `execo.process.TaktukProcess`, but will also be used as
          timeout for the underlying real taktuk process.
        """
        if local_files != None and (not hasattr(local_files, '__iter__')):
            local_files = (local_files,)
        super(TaktukRemote, self).__init__(name = name)
        self._caller_context = get_caller_context()
        self._hosts = get_hosts_list(hosts)
        self._processes = list()
        self._local_files = local_files
        self._remote_location = remote_location
        self._connexion_params = connexion_params
        self._other_kwargs = kwargs
        self._taktuk_stdout_output_handler = _TaktukPutOutputHandler(self)
        self._taktuk_stderr_output_handler = self._taktuk_stdout_output_handler
        self._taktuk_common_init()

    def _args(self):
        return [ repr(self._hosts),
                 repr(self._local_files) ] + Action._args(self) + TaktukPut._kwargs(self)

    def _kwargs(self):
        kwargs = []
        if self._remote_location: kwargs.append("remote_location=%r" % (self._remote_location,))
        if self._connexion_params: kwargs.append("connexion_params=%r" % (self._connexion_params,))
        for (k, v) in self._other_kwargs.iteritems(): kwargs.append("%s=%s" % (k, v))
        return kwargs

    def _infos(self):
        return TaktukRemote._infos(self)

    def name(self):
        if self._name == None:
            return "%s from %i hosts" % (self.__class__.__name__, len(self._hosts))
        else:
            return self._name

    def _gen_taktukprocesses(self):
        lifecycle_handler = ActionNotificationProcessLifecycleHandler(self, len(self._hosts))
        for host in self._hosts:
            process = TaktukProcess("",
                                    host = host,
                                    process_lifecycle_handler = lifecycle_handler,
                                    **self._other_kwargs)
            process._num_transfers_started = 0
            process._num_transfers_terminated = 0
            process._num_transfers_failed = 0
            self._processes.append(process)

    def _gen_taktuk_commands(self, hosts_with_explicit_user):
        self._taktuk_hosts_order = []
        for (index, host) in [ (idx, h) for (idx, h) in enumerate(self._hosts) if h not in hosts_with_explicit_user ]:
            self._taktuk_cmdline += ("-m", get_rewritten_host_address(host.address, self._connexion_params))
            self._taktuk_hosts_order.append(index)
        for (index, host) in [ (idx, h) for (idx, h) in enumerate(self._hosts) if h in hosts_with_explicit_user ]:
            self._taktuk_cmdline += ("-l", host.user, "-m", get_rewritten_host_address(host.address, self._connexion_params))
            self._taktuk_hosts_order.append(index)
        for src in self._local_files:
            self._taktuk_cmdline += ("broadcast", "put", "[", src, "]", "[", self._remote_location, "]", ";")

    def reset(self):
        retval = super(TaktukPut, self).reset()
        for process in self._processes:
            process._num_transfers_started = 0
            process._num_transfers_terminated = 0
            process._num_transfers_failed = 0
        return retval

class _TaktukGetOutputHandler(_TaktukRemoteOutputHandler):

    """Parse taktuk output."""

    def _update_taktukprocess_end_state(self, process):
        if process._num_transfers_started > 0 and not process.started():
            process.start()
        if process._num_transfers_failed + process._num_transfers_terminated >= len(self._taktukaction._remote_files):
            if process._num_transfers_failed > 0:
                process._set_terminated(error = True, error_reason = "taktuk file reception failed")
            else:
                process._set_terminated(exit_code = 0)
    
    def read_line(self, process, string, eof = False, error = False):
        try:
            if len(string) > 0:
                header = ord(string[0])
                (position, _, line) = string[2:].partition(" # ")
                position = int(position)
                if header in (68, 69): # connector, state
                    (peer_position, _, line) = line.partition(" # ")
                    if header == 68: # connector
                        peer_position = int(peer_position)
                        process = self._taktukaction._processes[self._taktukaction._taktuk_hosts_order[peer_position-1]]
                        process._handle_stderr(line)
                    else: # state
                        (state_code, _, _) = line.partition(" # ")
                        state_code = int(state_code)
                        if state_code in (13, 14, 15): # file reception started, failed, terminated
                            peer_position = int(peer_position)
                            process = self._taktukaction._processes[self._taktukaction._taktuk_hosts_order[peer_position-1]]
                            if state_code == 13: # file reception started
                                process._num_transfers_started += 1
                            elif state_code == 14: # file reception failed
                                process._num_transfers_failed += 1
                            else: # 15: file reception terminated
                                process._num_transfers_terminated += 1
                            self._update_taktukprocess_end_state(process)
                        elif state_code == 3 or state_code == 5: # connexion failed or lost
                            peer_position = int(peer_position)
                            process = self._taktukaction._processes[self._taktukaction._taktuk_hosts_order[peer_position-1]]
                            if state_code == 3: # connexion failed
                                process._set_terminated(error = True, error_reason = "taktuk connexion failed")
                            else: # 5: connexion lost
                                process._set_terminated(error = True, error_reason = "taktuk connexion lost")
                        elif state_code in (0, 1, 2, 4, 19):
                            pass
                        else:
                            self._log_unexpected_output(string)
                else:
                    self._log_unexpected_output(string)
        except Exception, e: #IGNORE:W0703
            logger.critical("%s: Unexpected exception %s while parsing taktuk output. Please report this message.", self.__class__.__name__, e)
            logger.critical("line received = %s", string.rstrip('\n'))

class TaktukGet(TaktukRemote):

    """Copy remote files from several remote host to a local directory, with ``taktuk``."""

    def __init__(self, hosts, remote_files, local_location = ".", connexion_params = None, name = None, **kwargs):
        """
        :param hosts: iterable of `execo.host.Host` from which to get
          the files.

        :param remote_files: an iterable of string of file
          paths. Substitions described in
          `execo.substitutions.remote_substitute` will not be
          performed, but taktuk substitutions can be used (see
          http://taktuk.gforge.inria.fr/taktuk.html#item_get__2a_src__2a__2a_dest__2a)

        :param local_location: the local directory were the files will
          be copied. Substitions described in
          `execo.substitutions.remote_substitute` will not be
          performed, but taktuk substitutions can be used (see
          http://taktuk.gforge.inria.fr/taktuk.html#item_get__2a_src__2a__2a_dest__2a)

        :param connexion_params: a dict similar to
          `execo.config.default_connexion_params` whose values will
          override those in default_connexion_params for connexion.

        :param name: action's name

        :param kwargs: passed to the instanciated
          `execo.process.TaktukProcess`. Special case for the
          ``timeout`` keyword argument: it will be passed to the dummy
          `execo.process.TaktukProcess`, but will also be used as
          timeout for the underlying real taktuk process.
        """
        if remote_files != None and (not hasattr(remote_files, '__iter__')):
            remote_files = (remote_files,)
        super(TaktukRemote, self).__init__(name = name)
        self._caller_context = get_caller_context()
        self._hosts = get_hosts_list(hosts)
        self._processes = list()
        self._remote_files = remote_files
        self._local_location = local_location
        self._connexion_params = connexion_params
        self._other_kwargs = kwargs
        self._taktuk_stdout_output_handler = _TaktukGetOutputHandler(self)
        self._taktuk_stderr_output_handler = self._taktuk_stdout_output_handler
        self._taktuk_common_init()

    def _args(self):
        return [ repr(self._hosts),
                 repr(self._remote_files) ] + Action._args(self) + TaktukGet._kwargs(self)

    def _kwargs(self):
        kwargs = []
        if self._local_location: kwargs.append("local_location=%r" % (self._local_location,))
        if self._connexion_params: kwargs.append("connexion_params=%r" % (self._connexion_params,))
        for (k, v) in self._other_kwargs.iteritems(): kwargs.append("%s=%s" % (k, v))
        return kwargs

    def _infos(self):
        return TaktukRemote._infos(self)

    def name(self):
        if self._name == None:
            return "%s from %i hosts" % (self.__class__.__name__, len(self._hosts))
        else:
            return self._name

    def _gen_taktukprocesses(self):
        lifecycle_handler = ActionNotificationProcessLifecycleHandler(self, len(self._hosts))
        for host in self._hosts:
            process = TaktukProcess("",
                                    host = host,
                                    process_lifecycle_handler = lifecycle_handler,
                                    **self._other_kwargs)
            process._num_transfers_started = 0
            process._num_transfers_terminated = 0
            process._num_transfers_failed = 0
            self._processes.append(process)

    def _gen_taktuk_commands(self, hosts_with_explicit_user):
        self._taktuk_hosts_order = []
        for (index, host) in [ (idx, h) for (idx, h) in enumerate(self._hosts) if h not in hosts_with_explicit_user ]:
            self._taktuk_cmdline += ("-m", get_rewritten_host_address(host.address, self._connexion_params))
            self._taktuk_hosts_order.append(index)
        for (index, host) in [ (idx, h) for (idx, h) in enumerate(self._hosts) if h in hosts_with_explicit_user ]:
            self._taktuk_cmdline += ("-l", host.user, "-m", get_rewritten_host_address(host.address, self._connexion_params))
            self._taktuk_hosts_order.append(index)
        for src in self._remote_files:
            self._taktuk_cmdline += ("broadcast", "get", "[", src, "]", "[", self._local_location, "]", ";")

    def reset(self):
        retval = super(TaktukGet, self).reset()
        for process in self._processes:
            process._num_transfers_started = 0
            process._num_transfers_terminated = 0
            process._num_transfers_failed = 0
        return retval

class Local(Action):

    """Launch a command localy."""

    def __init__(self, cmd, name = None, **kwargs):
        """
        :param cmd: the command to run.

        :param name: action's name

        :param kwargs: passed to the instanciated
          `execo.process.Process`.
        """
        super(Local, self).__init__(name = name)
        self._cmd = cmd
        self._other_kwargs = kwargs
        self._process = Process(self._cmd,
                                process_lifecycle_handler = ActionNotificationProcessLifecycleHandler(self, 1),
                                **self._other_kwargs)

    def _args(self):
        return [ repr(self._cmd) ] + Action._args(self) + Local._kwargs(self)

    def _kwargs(self):
        kwargs = []
        for (k, v) in self._other_kwargs.iteritems(): kwargs.append("%s=%s" % (k, v))
        return kwargs

    def _infos(self):
        return Action._infos(self)

    def name(self):
        if self._name == None:
            return "%s %s" % (self.__class__.__name__, self._cmd)
        else:
            return self._name

    def processes(self):
        return [ self._process ]

    def start(self):
        retval = super(Local, self).start()
        self._process.start()
        return retval

    def kill(self):
        retval = super(Local, self).kill()
        if self._process.running():
            self._process.kill()
        return retval

    def reset(self):
        retval = super(Local, self).reset()
        self._process.reset()
        return retval

class ParallelSubActionLifecycleHandler(ActionLifecycleHandler):

    def __init__(self, parallelaction, total_parallel_subactions):
        super(ParallelSubActionLifecycleHandler, self).__init__()
        self._parallelaction = parallelaction
        self._total_parallel_subactions = total_parallel_subactions
        self._terminated_subactions = 0

    def reset(self, action):
        self._terminated_subactions = 0

    def end(self, action):
        self._terminated_subactions += 1
        logger.debug("%i/%i subactions terminated in %s",
            self._terminated_subactions,
            self._total_parallel_subactions,
            self._parallelaction)
        if self._terminated_subactions == self._total_parallel_subactions:
            self._parallelaction._notify_terminated()

class ParallelActions(Action):

    """An `execo.action.Action` running several sub-Actions in parallel.

    Will start, kill, wait, run every action in parallel.
    """

    def __init__(self, actions, name = None):
        super(ParallelActions, self).__init__(name = name)
        self._actions = list(actions)
        self._subactions_lifecycle_handler = ParallelSubActionLifecycleHandler(self, len(self._actions))
        for action in self._actions:
            action.add_lifecycle_handler(self._subactions_lifecycle_handler)

    def _args(self):
        return [ repr(self._actions) ] + Action._args(self) + ParallelActions._kwargs(self)

    def _kwargs(self):
        return []

    def _infos(self):
        return Action._infos(self)

    def name(self):
        if self._name == None:
            return "%s %i actions" % (self.__class__.__name__, len(self._actions))
        else:
            return self._name

    def actions(self):
        """Return an iterable of `execo.action.Action` that this ParallelActions gathers."""
        return self._actions

    def start(self):
        retval = super(ParallelActions, self).start()
        for action in self._actions:
            action.start()
        return retval

    def kill(self):
        retval = super(ParallelActions, self).kill()
        for action in self._actions:
            action.kill()
        return retval

    def reset(self):
        retval = super(ParallelActions, self).reset()
        for action in self._actions:
            action.reset()
        return retval

    def processes(self):
        p = []
        for action in self._actions:
            p.extend(action.processes())
        return p

    def stats(self):
        stats = Report.empty_stats()
        stats['sub_stats'] = [action.stats() for action in self.actions()]
        Report.aggregate_stats(stats)
        return stats

class SequentialSubActionLifecycleHandler(ActionLifecycleHandler):

    def __init__(self, sequentialaction, index, total, next_subaction):
        super(SequentialSubActionLifecycleHandler, self).__init__()
        self._sequentialaction = sequentialaction
        self._index = index
        self._total = total
        self._next_subaction = next_subaction

    def end(self, action):
        logger.debug("%i/%i subactions terminated in %s",
            self._index,
            self._total,
            self._sequentialaction)
        if self._next_subaction:
            self._next_subaction.start()
        else:
            self._sequentialaction._notify_terminated()

class SequentialActions(Action):

    """An `execo.action.Action` running several sub-actions sequentially.

    Will start, kill, wait, run every Action sequentially.
    """

    def __init__(self, actions, name = None):
        super(SequentialActions, self).__init__(name = name)
        self._actions = list(actions)
        for (index, action) in enumerate(self._actions):
            if index + 1 < len(self._actions):
                next_action = self._actions[index + 1]
            else:
                next_action = None
            action.add_lifecycle_handler(SequentialSubActionLifecycleHandler(self,
                                                                             index,
                                                                             len(self._actions),
                                                                             next_action))

    def _args(self):
        return [ repr(self._actions) ] + Action._args(self) + SequentialActions._kwargs(self)

    def _kwargs(self):
        return []

    def _infos(self):
        return Action._infos(self)

    def name(self):
        if self._name == None:
            return "%s %i actions" % (self.__class__.__name__, len(self._actions))
        else:
            return self._name

    def actions(self):
        """Return an iterable of `execo.action.Action` that this SequentialActions gathers."""
        return self._actions

    def start(self):
        retval = super(SequentialActions, self).start()
        self._actions[0].start()
        return retval

    def kill(self):
        retval = super(SequentialActions, self).kill()
        for action in self._actions:
            action.kill()
        return retval

    def reset(self):
        retval = super(SequentialActions, self).reset()
        for action in self._actions:
            action.reset()
        return retval

    def processes(self):
        p = []
        for action in self._actions:
            p.extend(action.processes())
        return p

    def stats(self):
        stats = Report.empty_stats()
        stats['sub_stats'] = [action.stats() for action in self.actions()]
        Report.aggregate_stats(stats)
        return stats

class ActionFactory:
    """Instanciate multiple remote process execution and file copies using configurable connector tools: ``ssh``, ``scp``, ``taktuk``"""

    def __init__(self, remote_tool = None, fileput_tool = None, fileget_tool = None):
        """
        :param remote_tool: can be `execo.config.SSH` or
          `execo.config.TAKTUK`

        :param fileput_tool: can be `execo.config.SCP` or
          `execo.config.TAKTUK`

        :param fileget_tool: can be `execo.config.SCP` or
          `execo.config.TAKTUK`
        """

        if remote_tool == None:
            remote_tool = configuration.get("remote_tool")
        if remote_tool not in [SSH, TAKTUK]:
            raise KeyError, "no such remote tool: %s" % remote_tool
        if fileput_tool == None:
            fileput_tool = configuration.get("fileput_tool")
        if fileput_tool not in [SCP, TAKTUK]:
            raise KeyError, "no such fileput tool: %s" % fileput_tool
        if fileget_tool == None:
            fileget_tool = configuration.get("fileget_tool")
        if fileget_tool not in [SCP, TAKTUK]:
            raise KeyError, "no such fileget tool: %s" % fileget_tool
        self.remote_tool = remote_tool
        self.fileput_tool = fileput_tool
        self.fileget_tool = fileget_tool

    def remote(self, *args, **kwargs):
        """Instanciates a `execo.action.Remote` or `execo.action.TaktukRemote`"""
        if self.remote_tool == SSH:
            return Remote(*args, **kwargs)
        elif self.remote_tool == TAKTUK:
            return TaktukRemote(*args, **kwargs)

    def fileput(self, *args, **kwargs):
        """Instanciates a `execo.action.Put` or `execo.action.TaktukPut`"""
        if self.fileput_tool == SCP:
            return Put(*args, **kwargs)
        elif self.fileput_tool == TAKTUK:
            return TaktukPut(*args, **kwargs)

    def fileget(self, *args, **kwargs):
        """Instanciates a `execo.action.Get` or `execo.action.TaktukGet`"""
        if self.fileget_tool == SCP:
            return Get(*args, **kwargs)
        elif self.fileget_tool == TAKTUK:
            return TaktukGet(*args, **kwargs)

_default_action_factory = ActionFactory()

def get_remote(*args, **kwargs):
    """Instanciates a `execo.action.Remote` or `execo.action.TaktukRemote` with the default factory"""
    return _default_action_factory.remote(*args, **kwargs)

def get_fileput(*args, **kwargs):
    """Instanciates a `execo.action.Put` or `execo.action.TaktukPut` with the default factory"""
    return _default_action_factory.fileput(*args, **kwargs)

def get_fileget(*args, **kwargs):
    """Instanciates a `execo.action.Get` or `execo.action.TaktukGet` with the default factory"""
    return _default_action_factory.fileget(*args, **kwargs)
