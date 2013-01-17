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

import inspect
import re

def get_caller_context():
    """Return a tuple with (globals, locals) of the calling context."""
    return (inspect.stack()[2][0].f_globals, inspect.stack()[2][0].f_locals)

def remote_substitute(string, all_hosts, index, frame_context):
    """Perform some tag substitutions in a specific context.

    :param string: the string onto which to perfom the substitution.

    :param all_hosts: an iterable of `execo.host.Host` which is the
      context into which the substitution will be
      made. all_hosts[index] is the `execo.host.Host` to which this
      string applies.

    :param index: the index in all_hosts of the `execo.host.Host` to
      which this string applies.

    :param frame_context: a tuple of mappings (globals, locals) in the
      context of which the expressions (if any) will be evaluated.

    - Replaces all occurences of the literal string ``{{{host}}}`` by
      the `execo.host.Host` address itself.

    - Replaces all occurences of ``{{<expression>}}`` in the following
      way: ``<expression>`` must be a python expression, which will be
      evaluated in the context of frame_context (globals and locals),
      and which must return a sequence. ``{{<expression>}}`` will be
      replaced by ``<expression>[index % len(<expression>)]``.
    """

    def _subst_iterable(matchobj):
        sequence = eval(matchobj.group(1), frame_context[0], frame_context[1])
        if not hasattr(sequence, '__len__') or not hasattr(sequence, '__getitem__'):
            raise ValueError, "substitution of %s: %s must evaluate to a sequence" % (matchobj.group(), sequence)
        return str(sequence[index % len(sequence)])

    string = re.sub("\{\{\{host\}\}\}", all_hosts[index].address, string)
    string = re.sub("\{\{((?:(?!\}\}).)+)\}\}", _subst_iterable, string)
    return string
