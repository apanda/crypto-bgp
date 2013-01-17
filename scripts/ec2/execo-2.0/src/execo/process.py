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

from conductor import synchronized, the_conductor
from config import configuration
from execo.config import make_connexion_params
from execo.host import Host
from log import set_style, logger
from pty import openpty
from ssh_utils import get_ssh_command, get_rewritten_host_address
from time_utils import format_unixts, get_seconds
from utils import compact_output
import errno
import os
import re
import shlex
import signal
import subprocess
import threading
import time

class ProcessLifecycleHandler(object):

    """Abstract handler for `execo.process.ProcessBase` lifecycle."""

    def start(self, process):
        """Handle `execo.process.ProcessBase`'s start.

        :param process: The ProcessBase which starts.
        """
        pass

    def end(self, process):
        """Handle  `execo.process.ProcessBase`'s end.

        :param process: The ProcessBase which ends.
        """
        pass

    def reset(self, process):
        """Handle `execo.process.ProcessBase`'s reset.

        :param process: The ProcessBase which is reset.
        """
        pass

class ProcessOutputHandler(object):
    
    """Abstract handler for `execo.process.ProcessBase` output."""

    def __init__(self):
        """ProcessOutputHandler constructor. Call it in inherited classes."""
        self.__buffer = ""

    def read(self, process, string, eof = False, error = False):
        """Handle string read from a `execo.process.ProcessBase`'s stream.

        :param process: the ProcessBase which outputs the string

        :param string: the string read

        :param eof:(boolean) true if the stream is now at eof.
        
        :param error: (boolean) true if there was an error on the
          stream
        """
        self.__buffer += string
        while True:
            (line, sep, remaining) = self.__buffer.partition('\n')
            if remaining != '':
                self.read_line(process, line + sep)
                self.__buffer = remaining
            else:
                break
        if eof or error:
            self.read_line(process, self.__buffer, eof, error)
            self.__buffer = ""

    def read_line(self, process, string, eof = False, error = False):
        """Handle string read line by line from a `execo.process.ProcessBase`'s stream.

        :param process: the ProcessBase which outputs the line

        :param string: the line read

        :param eof:(boolean) true if the stream is now at eof.
        
        :param error: (boolean) true if there was an error on the
          stream
        """
        pass

class ProcessBase(object):

    """An almost abstract base class for all kinds of processes.

    There are no abstract methods, but a `execo.process.ProcessBase`
    by itself is almost useless, it only provides accessors to data
    shared with all subclasses, but no way to start it or stop
    it. These methods have to be implemented in concrete subclasses.

    It is possible to register custom lifecycle and output handlers to
    the process, in order to provide specific actions or stdout/stderr
    parsing when needed. See `execo.process.ProcessLifecycleHandler`
    and `execo.process.ProcessOutputHandler`.
    """

    def __init__(self, cmd, timeout = None, stdout_handler = None, stderr_handler = None,
                 ignore_exit_code = False, log_exit_code = None,
                 ignore_timeout = False, log_timeout = None,
                 ignore_error = False, log_error = None,
                 default_stdout_handler = True, default_stderr_handler = True,
                 process_lifecycle_handler = None):
        """
        :param cmd: string or tuple containing the command and args to
          run.

        :param timeout: timeout (in seconds, or None for no timeout)
          after which the process will automatically be sent a SIGTERM

        :param stdout_handler: instance of
          `execo.process.ProcessOutputHandler` for handling activity
          on process stdout

        :param stderr_handler: instance of
          `execo.process.ProcessOutputHandler` for handling activity
          on process stderr

        :param ignore_exit_code: if True, a process with a return code
          != 0 will still be considered ok

        :param log_exit_code: if True, termination of a process with a
          return code != 0 will cause a warning in logs

        :param ignore_timeout: if True, a process which reaches its
          timeout will be sent a SIGTERM, but will still be considered
          ok (but it will most probably have an exit code != 0)

        :param log_timeout: if True, a process which reaches its
          timeout and is sent a SIGTERM will cause a warning in logs

        :param ignore_error: if True, a process raising an OS level
          error will still be considered ok

        :param log_error: if True, a process raising an OS level
          error will cause a warning in logs

        :param default_stdout_handler: if True, a default handler
          sends stdout stream output to the member string accessed
          with self.stdout(). Default: True.

        :param default_stderr_handler: if True, a default handler
          sends stderr stream output to the member string accessed
          with self.stderr(). Default: True.

        :param process_lifecycle_handler: instance of
          `execo.process.ProcessLifecycleHandler` for being notified
          of process lifecycle events.
        """
        self._started = False
        self._start_date = None
        self._ended = False
        self._end_date = None
        self._error = False
        self._error_reason = None
        self._exit_code = None
        self._timeout_date = None
        self._timeouted = False
        self._forced_kill = False
        self._stdout = ""
        self._stderr = ""
        self._stdout_ioerror = False
        self._stderr_ioerror = False
        self._lock = threading.RLock()
        self._cmd = cmd
        self._timeout = timeout
        self._ignore_exit_code = ignore_exit_code
        self._ignore_timeout = ignore_timeout
        self._ignore_error = ignore_error
        self._log_exit_code = log_exit_code
        self._log_timeout = log_timeout
        self._log_error = log_error
        self._stdout_handler = stdout_handler
        self._stderr_handler = stderr_handler
        self._default_stdout_handler = default_stdout_handler
        self._default_stderr_handler = default_stderr_handler
        self._process_lifecycle_handler = process_lifecycle_handler

    def _common_reset(self):
        # all methods _common_reset() of this class hierarchy contain
        # the code common to the constructor and reset() to
        # reinitialize an object. If redefined in a child class,
        # _common_reset() must then explicitely call _common_reset()
        # of its parent class.
        self._started = False
        self._start_date = None
        self._ended = False
        self._end_date = None
        self._error = False
        self._error_reason = None
        self._exit_code = None
        self._timeout_date = None
        self._timeouted = False
        self._forced_kill = False
        self._stdout = ""
        self._stderr = ""
        self._stdout_ioerror = False
        self._stderr_ioerror = False

    def _args(self):
        # to be implemented in all subclasses. Must return a list with
        # all arguments to the constructor, beginning by the
        # positionnal arguments, finishing by keyword arguments. This
        # list will be directly used in __repr__ methods.
        return [ repr(self._cmd) ] + ProcessBase._kwargs(self)

    def _kwargs(self):
        # to be implemented in all subclasses. Must return a list with
        # all keyword arguments to the constructor. This list will be
        # used to build the list returned by _args() of this class or
        # child classes.
        kwargs = []
        if self._timeout: kwargs.append("timeout=%r" % (self._timeout,))
        if self._stdout_handler: kwargs.append("stdout_handler=%r" % (self._stdout_handler,))
        if self._stderr_handler: kwargs.append("stderr_handler=%r" % (self._stderr_handler,))
        if self._ignore_exit_code != False: kwargs.append("ignore_exit_code=%r" % (self._ignore_exit_code,))
        if self._ignore_timeout != False: kwargs.append("ignore_timeout=%r" % (self._ignore_timeout,))
        if self._ignore_error != False: kwargs.append("ignore_error=%r" % (self._ignore_error,))
        if self._log_exit_code != None: kwargs.append("log_exit_code=%r" % (self._log_exit_code,))
        if self._log_timeout != None: kwargs.append("log_timeout=%r" % (self._log_timeout,))
        if self._log_error != None: kwargs.append("log_error=%r" % (self._log_error,))
        if self._default_stdout_handler != True: kwargs.append("default_stdout_handler=%r" % (self._default_stdout_handler,))
        if self._default_stderr_handler != True: kwargs.append("default_stderr_handler=%r" % (self._default_stderr_handler,))
        if self._process_lifecycle_handler: kwargs.append("process_lifecycle_handler=%r" % (self._process_lifecycle_handler,))
        return kwargs

    def _infos(self):
        # to be implemented in all subclasses. Must return a list with
        # all relevant infos other than those returned by _args(), for
        # use in __str__ methods.
        return [ "started=%s" % (self._started,),
                 "start_date=%s" % (format_unixts(self._start_date),),
                 "ended=%s" % (self._ended,),
                 "end_date=%s" % (format_unixts(self._end_date),),
                 "error=%s" % (self._error,),
                 "error_reason=%s" % (self._error_reason,),
                 "timeouted=%s" % (self._timeouted,),
                 "exit_code=%s" % (self._exit_code,),
                 "ok=%s" % (self.ok(),) ]

    @synchronized
    def __repr__(self):
        # implemented once for all subclasses
        return "%s(%s)" % (self.__class__.__name__, ", ".join(self._args()))

    @synchronized
    def __str__(self):
        # implemented once for all subclasses
        return "<" + set_style(self.__class__.__name__, 'object_repr') + "(%s)>" % (", ".join(self._args() + self._infos()),)

    @synchronized
    def dump(self):
        return " %s\n" % (str(self),)+ set_style("stdout:", 'emph') + "\n%s\n" % (compact_output(self._stdout),) + set_style("stderr:", 'emph') + "\n%s" % (compact_output(self._stderr),)

    def cmd(self):
        """Return the process command line."""
        return self._cmd
    
    def started(self):
        """Return a boolean indicating if the process was started or not."""
        return self._started
    
    def start_date(self):
        """Return the process start date or None if not yet started."""
        return self._start_date
    
    def ended(self):
        """Return a boolean indicating if the process ended or not."""
        return self._ended
    
    def end_date(self):
        """Return the process end date or None if not yet ended."""
        return self._end_date

    @synchronized
    def running(self):
        """Return a boolean indicating if the process is currently running."""
        return self._started and not self._ended
    
    def error(self):
        """Return a boolean indicating if there was an error starting the process.

        This is *not* the process's return code.
        """
        return self._error
    
    def error_reason(self):
        """Return the operating system level errno, if there was an error starting the process, or None."""
        return self._error_reason
    
    def exit_code(self):
        """Return the process exit code.

        If available (if the process ended correctly from the OS point
        of view), or None.
        """
        return self._exit_code
    
    def timeout(self):
        """Return the timeout in seconds after which the process would be killed."""
        return self._timeout
    
    def timeout_date(self):
        """Return the date at which the process will reach its timeout.

        Or none if not available.
        """
        return self._timeout_date

    def timeouted(self):
        """Return a boolean indicating if the process has reached its timeout.

        Or None if we don't know yet (process still running, timeout
        not reached).
        """
        return self._timeouted
    
    def forced_kill(self):
        """Return a boolean indicating if the process was killed forcibly.

        When a process is killed with SIGTERM (either manually or
        automatically, due to reaching a timeout), execo will wait
        some time (constant set in execo source) and if after this
        timeout the process is still running, it will be killed
        forcibly with a SIGKILL.
        """
        return self._forced_kill
    
    def stdout(self):
        """Return a string containing the process stdout."""
        return self._stdout

    def stderr(self):
        """Return a string containing the process stderr."""
        return self._stderr

    def stdout_handler(self):
        """Return this process stdout `execo.process.ProcessOutputHandler`."""
        return self._stdout_handler
    
    def stderr_handler(self):
        """Return this process stderr `execo.process.ProcessOutputHandler`."""
        return self._stderr_handler

    def _handle_stdout(self, string, eof = False, error = False):
        """Handle stdout activity.

        :param string: available stream output in string

        :param eof: True if end of file on stream

        :param error: True if error on stream
        """
        if self._default_stdout_handler:
            self._stdout += string
        if error == True:
            self._stdout_ioerror = True
        if self._stdout_handler != None:
            self._stdout_handler.read(self, string, eof, error)
        
    def _handle_stderr(self, string, eof = False, error = False):
        """Handle stderr activity.

        :param string: available stream output in string

        :param eof: True if end of file on stream

        :param error: True if error on stream
        """
        if self._default_stderr_handler:
            self._stderr += string
        if error == True:
            self._stderr_ioerror = True
        if self._stderr_handler != None:
            self._stderr_handler.read(self, string, eof, error)

    @synchronized
    def ok(self):
        """Check process is ok.

        A process is ok, if:

        - it is not yet started or not yet ended

        - it started and ended and:

          - has no error (or was instructed to ignore them)

          - did not timeout (or was instructed to ignore it)

          - returned 0 (or was instructed to ignore a non zero exit
            code)
        """
        if not self._started: return True
        if self._started and not self._ended: return True
        return ((not self._error or self._ignore_error)
                and (not self._timeouted or self._ignore_timeout)
                and (self._exit_code == 0 or self._ignore_exit_code))

    @synchronized
    def finished_ok(self):
        """Check process has ran and is ok.

        A process is finished_ok if it has started and ended and
        it is ok.
        """
        return self._started and self._ended and self.ok()

    @synchronized
    def _log_terminated(self):
        """To be called (in subclasses) when a process terminates.

        This method will log process termination as needed.
        """
        s = set_style("terminated:", 'emph') + self.dump()
        if ((self._error and (self._log_error if self._log_error != None else not self._ignore_error))
            or (self._timeouted and (self._log_timeout if self._log_timeout != None else not self._ignore_timeout))
            or (self._exit_code != 0 and (self._log_exit_code if self._log_exit_code != None else not self._ignore_exit_code))):
            logger.warning(s)
        else:
            logger.debug(s)

    def kill(self, sig = signal.SIGTERM, auto_sigterm_timeout = True):
        raise NotImplementedError

    def wait(self, timeout = None):
        raise NotImplementedError

    def reset(self):
        """Reinitialize a process so that it can later be restarted.

        If it is running, this method will first kill it then wait for
        its termination before reseting;
        """
        logger.debug(set_style("reset:", 'emph') + " %s", str(self))
        if self._started and not self._ended:
            self.kill()
            self.wait()
        if self._process_lifecycle_handler != None:
            self._process_lifecycle_handler.reset(self)
        self._common_reset()
        return self

def _get_childs(pid):
    childs = []
    try:
        s = subprocess.Popen(("ps", "--ppid", str(pid)), stdout=subprocess.PIPE).communicate()[0]
        tmp_childs = [ int(c) for c in re.findall("^\s*(\d+)\s+", s, re.MULTILINE) ]
        childs.extend(tmp_childs)
        for child in tmp_childs:
            childs.extend(_get_childs(child))
    except OSError:
        pass
    return childs

class Process(ProcessBase):

    r"""Handle an operating system process.

    In coordination with the internal _Conductor I/O and process
    lifecycle management thread which is started when execo is
    imported, this class allows creation, start, interruption (kill),
    and waiting (for the termination) of an operating system
    process. The subprocess output streams (stdout, stderr), as well
    as various informations about the subprocess and its state can be
    accessed asynchronously.

    Example usage of the process class: run an iperf server, and
    connect to it with an iperf client:

    >>> server = Process('iperf -s', ignore_exit_code = True).start()
    >>> client = Process('iperf -c localhost -t 2').start()
    >>> client.started()
    True
    >>> client.ended()
    False
    >>> client.wait()
    Process('iperf -c localhost -t 2', close_stdin=True)
    >>> client.ended()
    True
    >>> server.ended()
    False
    >>> server.kill()
    Process('iperf -s', ignore_exit_code=True, close_stdin=True)
    >>> server.wait()
    Process('iperf -s', ignore_exit_code=True, close_stdin=True)
    >>> server.ended()
    True
    """

    def __init__(self, cmd,
                 close_stdin = None,
                 shell = False,
                 pty = False,
                 kill_subprocesses = None,
                 **kwargs):
        """
        :param cmd: string or tuple containing the command and args to
          run.

        :param close_stdin: boolean. whether or not to close
          subprocess's stdin. If None (default value), automatically
          choose based on pty.

        :param shell: whether or not to use a shell to run the
          cmd. See ``subprocess.Popen``

        :param pty: open a pseudo tty and connect process's stdin and
          stdout to it (stderr is still connected as a pipe). Make
          process a session leader. If lacking permissions to send
          signals to the process, try to simulate sending control
          characters to its pty.

        :param kill_subprocesses: if True, signals are also sent to
          subprocesses. If None, automatically decide based on
          shell = True/False. 
        """
        super(Process, self).__init__(cmd, **kwargs)
        self._process = None
        self._pid = None
        self._already_got_sigterm = False
        self._ptymaster = None
        self._ptyslave = None
        self._shell = shell
        self._pty = pty
        if close_stdin == None:
            if self._pty:
                self._close_stdin = False
            else:
                self._close_stdin = True
        else:
            self._close_stdin = close_stdin
        self._kill_subprocesses = kill_subprocesses
        if self._shell == False and isinstance(self._cmd, str):
            self._cmd = shlex.split(self._cmd)

    def _common_reset(self):
        super(Process, self)._common_reset()
        self._process = None
        self._pid = None
        self._already_got_sigterm = False
        self._ptymaster = None
        self._ptyslave = None

    def _args(self):
        return ProcessBase._args(self) + Process._kwargs(self)

    def _kwargs(self):
        kwargs = []
        if self._close_stdin: kwargs.append("close_stdin=%r" % (self._close_stdin,))
        if self._shell != False: kwargs.append("shell=%r" % (self._shell,))
        if self._pty != False: kwargs.append("pty=%r" % (self._pty,))
        if self._kill_subprocesses != None: kwargs.append("kill_subprocesses=%r" % (self._kill_subprocesses,)) 
        return kwargs

    def _infos(self):
        return ProcessBase._infos(self) + [ "pid=%s" % (self._pid,),
                                            "forced_kill=%s" % (self._forced_kill,) ]
    
    def pid(self):
        """Return the subprocess's pid, if available (subprocess started) or None."""
        return self._pid
    
    @synchronized
    def stdout_fd(self):
        """Return the subprocess stdout filehandle.

        Or None if not available.
        """
        if self._process != None:
            if self._pty:
                return self._ptymaster
            else:
                return self._process.stdout.fileno()
        else:
            return None

    @synchronized
    def stderr_fd(self):
        """Return the subprocess stderr filehandle.

        Or None if not available.
        """
        if self._process != None:
            return self._process.stderr.fileno()
        else:
            return None

    @synchronized
    def stdin_fd(self):
        """Return the subprocess stdin filehandle.

        Or None if not available.
        """
        if self._process != None and not self._close_stdin:
            if self._pty:
                return self._ptymaster
            else:
                return self._process.stdin.fileno()
        else:
            return None

    def start(self):
        """Start the subprocess."""
        # not synchronized: instead, self._lock managed explicitely to
        # avoid deadlock with conductor lock
        with self._lock:
            if self._started:
                raise ValueError, "unable to start an already started process"
            logger.debug(set_style("start:", 'emph') + " %s", str(self))
            self._started = True
            self._start_date = time.time()
            if self._timeout != None:
                self._timeout_date = self._start_date + self._timeout
            if self._pty:
                (self._ptymaster, self._ptyslave) = openpty()
        if self._process_lifecycle_handler != None:
            self._process_lifecycle_handler.start(self)
        try:
            with the_conductor.get_lock():
                # this lock is needed to ensure that
                # Conductor.__update_terminated_processes() won't be
                # called before the process has been registered to the
                # conductor
                if self._pty:
                    self._process = subprocess.Popen(self._cmd,
                                                     stdin = self._ptyslave,
                                                     stdout = self._ptyslave,
                                                     stderr = subprocess.PIPE,
                                                     close_fds = True,
                                                     shell = self._shell,
                                                     preexec_fn = os.setsid)
                else:
                    self._process = subprocess.Popen(self._cmd,
                                                     stdin = subprocess.PIPE,
                                                     stdout = subprocess.PIPE,
                                                     stderr = subprocess.PIPE,
                                                     close_fds = True,
                                                     shell = self._shell)
                self._pid = self._process.pid
                the_conductor.add_process(self)
            if self._close_stdin:
                self._process.stdin.close()
        except OSError, e:
            with self._lock:
                self._error = True
                self._error_reason = e
                self._ended = True
                self._end_date = self._start_date
                self._log_terminated()
            if self._process_lifecycle_handler != None:
                self._process_lifecycle_handler.end(self)
        return self

    @synchronized
    def kill(self, sig = signal.SIGTERM, auto_sigterm_timeout = True):
        """Send a signal (default: SIGTERM) to the subprocess.

        :param sig: the signal to send

        :param auto_sigterm_timeout: whether or not execo will check
          that the subprocess has terminated after a preset timeout,
          when it has received a SIGTERM, and automatically send
          SIGKILL if the subprocess is not yet terminated
        """
        if self._pid != None and not self._ended:
            logger.debug(set_style("kill with signal %s:" % sig, 'emph') + " %s", str(self))
            if sig == signal.SIGTERM:
                self._already_got_sigterm = True
                if auto_sigterm_timeout == True:
                    self._timeout_date = time.time() + configuration.get('kill_timeout')
                    the_conductor.update_process(self)
            if sig == signal.SIGKILL:
                self._forced_kill = True
            if (self._kill_subprocesses == True
                or (self._kill_subprocesses == None
                    and self._shell == True)):
                additionnal_processes_to_kill = _get_childs(self._pid)
            else:
                additionnal_processes_to_kill = []
            try:
                os.kill(self._pid, sig)
            except OSError, e:
                if e.errno == errno.EPERM:
                    if (self._pty
                        and (sig == signal.SIGTERM
                             or sig == signal.SIGHUP
                             or sig == signal.SIGINT
                             or sig == signal.SIGKILL
                             or sig == signal.SIGPIPE
                             or sig == signal.SIGQUIT)):
                        # unable to send signal to process due to lack
                        # of permissions. If _pty == True, then there
                        # is a pty, we can close its master side, it should
                        # trigger a signal (SIGPIPE?) on the other side
                        os.close(self._ptymaster)
                        logger.debug("EPERM for signal %s -> closing pty master side of %s", sig, str(self))
                    else:
                        logger.debug(set_style("EPERM: unable to send signal", 'emph') + " to %s", str(self))
                elif e.errno == errno.ESRCH:
                    # process terminated so recently that self._ended
                    # has not been updated yet
                    pass
                else:
                    raise e
            for p in additionnal_processes_to_kill:
                try:
                    os.kill(p, sig)
                except OSError, e:
                    if e.errno == errno.EPERM or e.errno == errno.ESRCH:
                        pass
                    else:
                        raise e
        return self

    @synchronized
    def _timeout_kill(self):
        """Send SIGTERM to the subprocess, due to the reaching of its timeout.

        This method is intended to be used by the
        `execo.conductor._Conductor` thread.
        
        If the subprocess already got a SIGTERM and is still there, it
        is directly killed with SIGKILL.
        """
        if self._pid != None:
            self._timeouted = True
            if self._already_got_sigterm and self._timeout_date >= time.time():
                self.kill(signal.SIGKILL)
            else:
                self.kill()

    @synchronized
    def _set_terminated(self, exit_code):
        """Update process state: set it to terminated.

        This method is intended to be used by the
        `execo.conductor._Conductor` thread.

        Update its exit_code, end_date, ended flag, and log its
        termination (INFO or WARNING depending on how it ended).
        """
        logger.debug("set terminated %s, exit_code=%s", str(self), exit_code)
        self._exit_code = exit_code
        self._end_date = time.time()
        self._ended = True
        if self._ptymaster != None:
            try:
                os.close(self._ptymaster)
            except OSError, e:
                if e.errno == errno.EBADF: pass
                else: raise e
        if self._ptyslave != None:
            try:
                os.close(self._ptyslave)
            except OSError, e:
                if e.errno == errno.EBADF: pass
                else: raise e
        if self._process.stdin:
            self._process.stdin.close()
        if self._process.stdout:
            self._process.stdout.close()
        if self._process.stderr:
            self._process.stderr.close()
        self._log_terminated()
        if self._process_lifecycle_handler != None:
            self._process_lifecycle_handler.end(self)

    def wait(self, timeout = None):
        """Wait for the subprocess end."""
        with the_conductor.get_lock():
            if self._error:
                return self
            if not self._started or self._pid == None:
                raise ValueError, "Trying to wait a process which has not been started"
            logger.debug(set_style("wait:", 'emph') + " %s", str(self))
            timeout = get_seconds(timeout)
            if timeout != None:
                end = time.time() + timeout 
            while self._ended != True and (timeout == None or timeout > 0):
                the_conductor.get_condition().wait(timeout)
                if timeout != None:
                    timeout = end - time.time()
            logger.debug(set_style("wait finished:", 'emph') + " %s", str(self))
        return self

    def run(self, timeout = None):
        """Start subprocess then wait for its end."""
        return self.start().wait(timeout)

class SshProcess(Process):

    r"""Handle a remote command execution through ssh or similar remote execution tool.

    Note: the closing of the remote process upon killing of the
    SshProcess depends on the ssh (or ssh-like) command behavior. With
    openssh, this can be obtained by passing options -tt (force tty
    creation), thus these are the default options in
    ``execo.config.default_connexion_params``.
    """

    def __init__(self, cmd, host = None, connexion_params = None, **kwargs):
        """:param host: `execo.host.Host` to connect to

        :param connexion_params: connexion parameters

        :param kwargs: passed to `execo.process.Process`. Special case
          for keyword argument ``pty``: if this keyword argument is
          not given, use the ``pty`` field of the connexion params,
          else use the ``pty`` keyword argument, which takes
          precedence. This allows setting default pty behaviors in
          connexion_params shared by various remote processes (this
          was motivated by allowing
          `execo_g5k.config.default_oarsh_oarcp_params` to set pty to
          True, because oarsh/oarcp are run sudo which forbids to send
          signals).
        """
        self._host = Host(host)
        self._cmd = cmd
        self._connexion_params = connexion_params
        real_cmd = (get_ssh_command(self._host.user,
                                    self._host.keyfile,
                                    self._host.port,
                                    connexion_params)
                    + (get_rewritten_host_address(self._host.address, connexion_params),)
                    + (cmd,))
        if not 'pty' in kwargs:
            kwargs['pty'] = make_connexion_params(connexion_params).get('pty')
        super(SshProcess, self).__init__(real_cmd, **kwargs)

    def _args(self):
        return [ repr(self._host),
                 repr(self._cmd) ] + Process._kwargs(self) + SshProcess._kwargs(self)

    def _kwargs(self):
        kwargs = []
        if self._connexion_params: kwargs.append("connexion_params=%r" % (self._connexion_params,))
        return kwargs

    def _infos(self):
        return [ "real cmd=%r" % (self._cmd,) ] + Process._infos(self)

    def remote_cmd(self):
        """Return the command line executed remotely through ssh."""
        return self._cmd

    def host(self):
        """Return the remote host."""
        return self._host

    def connexion_params(self):
        """Return ssh connexion parameters."""
        return self._connexion_params

class TaktukProcess(ProcessBase): #IGNORE:W0223

    r"""Dummy process similar to `execo.process.SshProcess`."""

    def __init__(self, cmd, host = None, **kwargs):
        self._host = Host(host)
        self._cmd = cmd
        super(TaktukProcess, self).__init__(cmd, **kwargs)

    def _args(self):
        return [ repr(self._host),
                 repr(self._cmd) ] + ProcessBase._kwargs(self)

    def host(self):
        """Return the remote host."""
        return self._host

    @synchronized
    def start(self):
        """Notify TaktukProcess of actual remote process start.

        This method is intended to be used by
        `execo.action.TaktukRemote`.
        """
        if self._started:
            raise ValueError, "unable to start an already started process"
        logger.debug(set_style("start:", 'emph') + " %s", str(self))
        self._started = True
        self._start_date = time.time()
        if self._timeout != None:
            self._timeout_date = self._start_date + self._timeout
        if self._process_lifecycle_handler != None:
            self._process_lifecycle_handler.start(self)
        return self

    @synchronized
    def _set_terminated(self, exit_code = None, error = False, error_reason = None, timeouted = None, forced_kill = None):
        """Update TaktukProcess state: set it to terminated.

        This method is intended to be used by `execo.action.TaktukRemote`.

        Update its exit_code, end_date, ended flag, and log its
        termination (INFO or WARNING depending on how it ended).
        """
        if not self._started:
            self.start()
        logger.debug("set terminated %s, exit_code=%s, error=%s", str(self), exit_code, error)
        if error != None:
            self._error = error
        if error_reason != None:
            self._error_reason = error_reason
        if exit_code != None:
            self._exit_code = exit_code
        if timeouted == True:
            self._timeouted = True
        if forced_kill == True:
            self._forced_kill = True
        self._end_date = time.time()
        self._ended = True
        self._log_terminated()
        if self._process_lifecycle_handler != None:
            self._process_lifecycle_handler.end(self)

def get_process(*args, **kwargs):
    """Instanciates a `execo.process.Process` or `execo.process.SshProcess`, depending on the existence of host keyword argument"""
    if kwargs.get("host") != None:
        return SshProcess(*args, **kwargs)
    else:
        del kwargs["host"]
        if "connexion_params" in kwargs: del kwargs["connexion_params"]
        return Process(*args, **kwargs)
