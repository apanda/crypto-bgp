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

from log import logger
from subprocess import MAXFD
import optparse
import os
import sys
import time
import inspect

class ArgsOptionParser(optparse.OptionParser):

    """optparse.OptionParser subclass which keeps tracks of arguments for proper help string generation.

    This class is a rather quick and dirty hack. Using ``argparse``
    would be better but it's only available in python 2.7.
    """

    def __init__(self, *args, **kwargs):
        self.arguments = []
        optparse.OptionParser.__init__(self, *args, **kwargs)

    def add_argument(self, arg_name, description):
        """Add an arg_name to the arg_name list."""
        self.arguments.append((arg_name, description))

    def num_arguments(self):
        """Returns the number of expected arguments."""
        return len(self.arguments)

    def format_arguments(self, formatter=None):
        if formatter is None:
            formatter = self.formatter
        result = []
        result.append(formatter.format_heading("Arguments"))
        formatter.indent()
        for argument in self.arguments:
            result.append(formatter._format_text("%s: %s" % (argument[0], argument[1])))
            result.append("\n")
        formatter.dedent()
        result.append("\n")
        return "".join(result)

    def format_help(self, formatter=None):
        if formatter is None:
            formatter = self.formatter
        result = []
        if self.usage:
            result.append(self.get_usage() + "\n")
        if self.description:
            result.append(self.format_description(formatter) + "\n")
        result.append(self.format_arguments(formatter))
        result.append(self.format_option_help(formatter))
        result.append(self.format_epilog(formatter))
        return "".join(result)

def _redirect_fd(fileno, filename):
    # create and open file filename, and redirect open file fileno to it
    f = os.open(filename, os.O_CREAT | os.O_WRONLY | os.O_APPEND, 0644)
    os.dup2(f, fileno)
    os.close(f)

def _tee_fd(fileno, filename):
    # create and open file filename, and duplicate open file fileno to it
    pr, pw = os.pipe()
    pid = os.fork()
    if pid == 0:
        os.dup2(pr, 0)
        os.dup2(fileno, 1)
        if (os.sysconf_names.has_key("SC_OPEN_MAX")):
            maxfd = os.sysconf("SC_OPEN_MAX")
        else:
            maxfd = MAXFD
        os.closerange(3, maxfd)
        os.execv('/usr/bin/tee', ['tee', '-a', filename])
    else:
        os.close(pr)
        os.dup2(pw, fileno)
        os.close(pw)

def run_meth_on_engine_ancestors(instance, method_name):
    engine_ancestors = [ cls for cls in inspect.getmro(instance.__class__) if issubclass(cls, Engine) ]
    for cls in engine_ancestors:
        meth = cls.__dict__.get(method_name)
        if meth is not None:
            meth(instance)

class Engine(object):

    """Basic class for execo Engine.

    Subclass it to develop your own engines, possibly reusable.

    This class offers basic facilities:

    - central handling of options and arguments

    - automatic experiment directory creation

    - various ways to handle stdout / stderr

    - support for continuing a previously stopped experiment

    - log level selection

    The instanciation of the class is done with the ``execo-run``
    command line tool. The name of the engine (the name of the class
    inheriting from `execo_engine.engine.Engine`) is given as first
    argument to ``execo-run``, which will try to instanciate the class
    from file ``<classname>.py`` in the search path (run ``execo-run``
    without arguments to see the search path). Further arguments are
    passed to the engine's option parser.

    A subclass of Engine can access the following member variables
    which are automatically defined and initialized at the right time
    by the base class `execo_engine.engine.Engine`:

    - `execo_engine.engine.Engine.result_dir`

    - `execo_engine.engine.Engine.options_parser`

    - `execo_engine.engine.Engine.options`

    - `execo_engine.engine.Engine.args`

    - `execo_engine.engine.Engine.run_name`

    - `execo_engine.engine.Engine.result_dir`

    A subclass of Engine can override the following methods:

    - `execo_engine.engine.Engine.init`

    - `execo_engine.engine.Engine.run`

    - `execo_engine.engine.Engine.setup_run_name`

    - `execo_engine.engine.Engine.setup_result_dir`

    A typical, non-reusable engine would override
    `execo_engine.engine.Engine.init`, adding options / arguments to
    `execo_engine.engine.Engine.options_parser`, and override
    `execo_engine.engine.Engine.run`, putting all the experiment code
    inside it, being sure that all initializations are done when
    ``run`` is called: results directory is initialized and created
    (possibly reusing a previous results directory, to restart from a
    previously stopped experiment), log level is set, stdout / stderr
    are redirected as needed, and options and arguments are in
    `execo_engine.engine.Engine.options` and
    `execo_engine.engine.Engine.args`.

    A typical usage of a `execo_engine.utils.ParamSweeper` in an
    engine would be to intialize one at the beggining of
    `execo_engine.engine.Engine.run`, using a persistence file in the
    results directory. example code::

     def run(self):
         sweeps = sweep({<parameters and their values>})
         sweeper = ParamSweeper(sweeps, os.path.join(self.result_dir, "sweeps"))
         [...]

    """

    def _create_result_dir(self):
        """Ensure the engine's result dir exists. Create it if needed."""
        if not os.path.isdir(self.result_dir):
            os.makedirs(self.result_dir)

    def _redirect_outputs(self, merge_stdout_stderr):
        """Redirects, and optionnaly merge, stdout and stderr to file(s) in experiment directory."""

        if merge_stdout_stderr:
            stdout_redir_filename = self.result_dir + "/stdout+stderr"
            stderr_redir_filename = self.result_dir + "/stdout+stderr"
            logger.info("redirect stdout / stderr to %s", stdout_redir_filename)
        else:
            stdout_redir_filename = self.result_dir + "/stdout"
            stderr_redir_filename = self.result_dir + "/stderr"
            logger.info("redirect stdout / stderr to %s and %s", stdout_redir_filename, stderr_redir_filename)
        sys.stdout.flush()
        sys.stderr.flush()
        _redirect_fd(1, stdout_redir_filename)
        _redirect_fd(2, stderr_redir_filename)
        # additionnaly force stdout unbuffered by reopening stdout
        # file descriptor with write mode
        # and 0 as the buffer size (unbuffered)
        sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', 0)

    def _copy_outputs(self, merge_stdout_stderr):
        """Copy, and optionnaly merge, stdout and stderr to file(s) in experiment directory."""

        if merge_stdout_stderr:
            stdout_redir_filename = self.result_dir + "/stdout+stderr"
            stderr_redir_filename = self.result_dir + "/stdout+stderr"
        else:
            stdout_redir_filename = self.result_dir + "/stdout"
            stderr_redir_filename = self.result_dir + "/stderr"
        sys.stdout.flush()
        sys.stderr.flush()
        _tee_fd(1, stdout_redir_filename)
        _tee_fd(2, stderr_redir_filename)
        # additionnaly force stdout unbuffered by reopening stdout
        # file descriptor with write mode
        # and 0 as the buffer size (unbuffered)
        sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', 0)
        if merge_stdout_stderr:
            logger.info("dup stdout / stderr to %s", stdout_redir_filename)
        else:
            logger.info("dup stdout / stderr to %s and %s", stdout_redir_filename, stderr_redir_filename)
            
    def __init__(self):
        self.engine_dir = os.path.abspath(os.path.dirname(os.path.realpath(sys.modules[self.__module__].__file__)))
        """Full path of the engine directory. Available to client
        code, should not be modified (why would you want?)
        """
        self.options_parser = ArgsOptionParser()
        """An instance of
        `execo_engine.engine.ArgsOptionParser`. Subclasses of
        `execo_engine.engine.Engine` can register options and args to
        this options parser in `execo_engine.engine.Engine.init`.
        """
        self.options_parser.add_option(
            "-l", dest = "log_level", type = "int", default = 20,
            help = "log level (10=DEBUG, 20=INFO, 30=WARNING, 40=ERROR, 50=CRITICAL). Default = %default")
        self.options_parser.add_option(
            "-L", dest = "output_mode", action="store_const", const = "copy", default = False,
            help = "copy stdout / stderr to log files in the experiment result directory. Default = %default")
        self.options_parser.add_option(
            "-R", dest = "output_mode", action="store_const", const = "redirect", default = False,
            help = "redirect stdout / stderr to log files in the experiment result directory. Default = %default")
        self.options_parser.add_option(
            "-M", dest = "merge_outputs", action="store_true", default = False,
            help = "when copying or redirecting outputs, merge stdout / stderr in a single file. Default = %default")
        self.options_parser.add_option(
            "-c", dest = "continue_dir", default = None, metavar = "DIR",
            help = "continue experiment in DIR")
        self.options_parser.set_description("engine: " + self.__class__.__name__)
        self.options = None
        """Options given on the command line. Available after the
        command line has been parsed, in
        `execo_engine.engine.Engine.run` (not in
        `execo_engine.engine.Engine.init`)
        """
        self.args = None
        """Arguments given on the command line. Available after the
        command line has been parsed, in
        `execo_engine.engine.Engine.run` (not in
        `execo_engine.engine.Engine.init`)
        """
        self.run_name = None
        """Name of the current experiment. If you want to modify it,
        override `execo_engine.engine.Engine.setup_run_name`
        """
        self.result_dir = None
        """full path to the current engine's execution results
        directory, where results should be written, where stdout /
        stderr are output, where `execo_engine.utils.ParamSweeper`
        persistence files should be written, and where more generally
        any file pertaining to a particular execution of the
        experiment should be located.
        """

    def _start(self):
        """Start the engine.

        Properly initialize the experiment Engine instance, then call
        the init() method of all subclasses, then pass the control to
        the overridden run() method of the requested experiment
        Engine.
        """
        (self.options, self.args) = self.options_parser.parse_args()
        del self.args[0]
        logger.setLevel(self.options.log_level)
        if len(self.args) < self.options_parser.num_arguments():
            self.options_parser.print_help(sys.stderr)
            exit(1)
        self.setup_run_name()
        if self.options.continue_dir:
            if not os.path.isdir(self.options.continue_dir):
                print >> sys.stderr, "ERROR: unable to find experiment dir %s" % (self.options.continue_dir,)
                exit(1)
            self.result_dir = self.options.continue_dir
        else:
            self.setup_result_dir()
        self._create_result_dir()
        if self.options.output_mode:
            if self.options.output_mode == "copy":
                self._copy_outputs(self.options.merge_outputs)
            elif self.options.output_mode == "redirect":
                self._redirect_outputs(self.options.merge_outputs)
        logger.info("command line arguments: %s" % (sys.argv,))
        if self.options.continue_dir:
            logger.info("continue experiment in %s", self.options.continue_dir)
        run_meth_on_engine_ancestors(self, "init")
        run_meth_on_engine_ancestors(self, "run")

    # ------------------------------------------------------------------
    #
    # below: methods that inherited real experiment engines can
    # override
    #
    # ------------------------------------------------------------------

    def setup_run_name(self):
        """Set the experiment run name.

        Default implementation: concatenation of class name and
        date. Override this method to change the name of the
        experiment. This method is called before
        `execo_engine.engine.Engine.run` and
        `execo_engine.engine.Engine.init`
        """
        self.run_name = self.__class__.__name__ + "_" + time.strftime("%Y%m%d_%H%M%S_%z")

    def setup_result_dir(self):
        """Set the experiment run result directory name.

        Default implementation: subdirectory with the experiment run
        name in the current directory. Override this method to change
        the name of the result directory. This method is called before
        `execo_engine.engine.Engine.run` and
        `execo_engine.engine.Engine.init`. Note that if option ``-c``
        is given to the engine, the name given on command line will
        take precedence, and this method won't be called.
        """
        self.result_dir = os.path.abspath(self.run_name)

    def init(self):
        """Experiment init method

        Override this method with the experiment init code. Default
        implementation does nothing.

        The base class `execo_engine.engine.Engine` takes care that
        all `execo_engine.engine.Engine.init` methods of its subclass
        hierarchy are called, in the order ancestor method before
        subclass method. This order is chosen so that generic engines
        inheriting from `execo_engine.engine.Engine` can easily
        implement common functionnalities. For example a generic
        engine can declare its own options and arguments in ``init``,
        which will be executed before a particular experiment subclass
        ``init`` method.
        """
        pass

    def run(self):
        """Experiment run method

        Override this method with the experiment code. Default
        implementation does nothing.

        The base class `execo_engine.engine.Engine` takes care that
        all `execo_engine.engine.Engine.run` methods of its subclass
        hierarchy are called, in the order ancestor method before
        subclass method. This order is chosen so that generic engines
        inheriting from `execo_engine.engine.Engine` can easily
        implement common functionnalities. For example a generic
        engine can prepare an experiment environment in its ``run``
        method, which will be executed before a particular experiment
        subclass ``run`` method.
        """
        pass
