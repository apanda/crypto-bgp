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

import threading, os, cPickle
from log import logger

class HashableDict(dict):

    """Hashable dictionnary. Beware: must not mutate it after its first use as a key."""

    def __key(self):
        return tuple((k,self[k]) for k in sorted(self))

    def __hash__(self):
        return hash(self.__key())

    def __eq__(self, other):
        return other and self.__key() == other.__key()

def sweep(parameters):

    """Generates all combinations of parameters.

    The aim of this function is, given a list of experiment parameters
    (named factors), and for each parameter (factor), the list of
    their possible values (named levels), to generate the cartesian
    product of all parameter values (called a full factorial design in
    *The Art Of Computer Systems Performance Analysis, R. Jain, Wiley
    1991*).

    More formally: given a a dict associating factors as keys and the
    list of their possible levels as values, this function will return
    a list of dict corresponding to all cartesian product of all level
    combinations. Each dict in the returned list associates the
    factors as keys, and one of its possible levels as value.

    In the given factors dict, if for a factor X (key), the value
    associated is a dict instead of a list of levels, then it will use
    the keys of the sub-dict as levels for the factor X, and the
    values of the sub-dict must also be some dict for factor / levels
    combinations which will be explored only for the corresponding
    levels of factor X. This is kind of recursive sub-sweep and it
    allows to explore some factor / level combinations only for some
    levels of a given factor.

    The returned list contains `execo_engine.utils.HashableDict`
    instead of dict, which is a simple subclass of dict, so that
    parameters combinations can be used as dict keys (but don't modify
    them in such cases)

    Examples:

    >>> sweep({
    ...     "param 1": ["a", "b"],
    ...     "param 2": [1, 2]
    ...     })
    [{'param 1': 'a', 'param 2': 1}, {'param 1': 'a', 'param 2': 2}, {'param 1': 'b', 'param 2': 1}, {'param 1': 'b', 'param 2': 2}]

    >>> sweep({
    ...     "param 1": ["a", "b"],
    ...     "param 2": {
    ...         1: {
    ...             "param 1 1": [ "x", "y" ],
    ...             "param 1 2": [ 0.0, 1.0 ]
    ...             },
    ...         2: {
    ...             "param 2 1": [ -10, 10 ]
    ...             }
    ...         }
    ...     })
    [{'param 1 2': 0.0, 'param 1 1': 'x', 'param 1': 'a', 'param 2': 1}, {'param 1 2': 0.0, 'param 1 1': 'y', 'param 1': 'a', 'param 2': 1}, {'param 1 2': 1.0, 'param 1 1': 'x', 'param 1': 'a', 'param 2': 1}, {'param 1 2': 1.0, 'param 1 1': 'y', 'param 1': 'a', 'param 2': 1}, {'param 2 1': -10, 'param 1': 'a', 'param 2': 2}, {'param 2 1': 10, 'param 1': 'a', 'param 2': 2}, {'param 1 2': 0.0, 'param 1 1': 'x', 'param 1': 'b', 'param 2': 1}, {'param 1 2': 0.0, 'param 1 1': 'y', 'param 1': 'b', 'param 2': 1}, {'param 1 2': 1.0, 'param 1 1': 'x', 'param 1': 'b', 'param 2': 1}, {'param 1 2': 1.0, 'param 1 1': 'y', 'param 1': 'b', 'param 2': 1}, {'param 2 1': -10, 'param 1': 'b', 'param 2': 2}, {'param 2 1': 10, 'param 1': 'b', 'param 2': 2}]
    """

    result = [HashableDict()]
    for key, val in parameters.items():
        if len(val) == 0: continue
        newresult = []
        for i in result:
            if isinstance(val, dict):
                for subkey, subval in val.items():
                    for subcombs in sweep(subval):
                        subresult = HashableDict(i)
                        subresult.update({key: subkey})
                        subresult.update(subcombs)
                        newresult.append(subresult)
            else:
                for j in val:
                    subresult = HashableDict(i)
                    subresult.update({key: j})
                    newresult.append(subresult)
        result = newresult
    return result

class ParamSweeper(object):

    """Threadsafe and persistent iterable container to iterate over a list of experiment parameters (or whatever, actually).

    The aim of this class is to provide a convenient way to iterate
    over several experiment configurations (or anything else). It is
    an iterable container with the following characteristics:

    - each element of the iterable has three states:

      - *todo*

      - *done*

      - *skipped*

    - at beginning, each element is in state *todo*

    - client code can mark any element *done* or *skipped*

    - when iterating over it, you always get the next item in *todo*
      state

    - this container is threadsafe

    - this container has automatic persistence of the element states
      *done* (but not state *skipped*) to disk: If later you
      instanciate a container with the same persistence file (path to
      this file is given to constructor), then the elements of the
      container will be taken from the constructor argument, but those
      marked *done* in the last stored in the persistent file will be
      marked *done*

    This container is intended to be used in the following way: at the
    beginning of the experiment, you initialize a ParamSweeper with
    the list of experiment configurations (which can result from a
    call to `execo_engine.utils.sweep`, but not necessarily) and a
    filename for the persistence. During execution, you request
    (possibly from several threads) new experiment configurations with
    `execo_engine.utils.ParamSweeper.get_next`, mark them *done* or
    *skipped* with `execo_engine.utils.ParamSweeper.done` and
    `execo_engine.utils.ParamSweeper.skip`. At a later date, you can
    relaunch the same script, it will continue from where it left,
    also retrying the skipped configurations. This works well when
    used with `execo_engine.engine.Engine` startup option ``-c``
    (continue experiment in a given directory).
    """

    def __init__(self, sweeps, persistence_file, name = None):
        """:param sweeps: what to iterate on

        :param persistence_file: path to persistence file
        """
        self.__lock = threading.RLock()
        self.__sweeps = sweeps
        self.__done = set()
        self.__skipped = set()
        self.__inprogress = set()
        self.__remaining = frozenset()
        self.__done_file = persistence_file
        self.__name = name
        if not self.__name:
            self.__name = os.path.basename(self.__done_file)
        if os.path.isfile(self.__done_file):
            with open(self.__done_file, "r") as done_file:
                self.__done = cPickle.load(done_file)
        self.__update_remaining()

    def __update_remaining(self):
        self.__remaining = frozenset(self.__sweeps).difference(self.__done).difference(self.__skipped).difference(self.__inprogress)

    def __str__(self):
        return "%s <%i total, %i done, %i skipped, %i in progress, %i remaining>" % (
            self.__name,
            self.num_total(),
            self.num_done(),
            self.num_skipped(),
            self.num_inprogress(),
            self.num_remaining())

    def set_sweeps(self, sweeps):
        """Change the list of what to iterate on"""
        with self.__lock:
            self.__sweeps = sweeps
            self.__update_remaining()

    def get_next(self):
        """Return the next element which is *todo*. Returns None if reached end."""
        with self.__lock:
            try:
                combination = iter(self.__remaining).next()
                self.__inprogress.add(combination)
                self.__update_remaining()
                logger.info("%s new combination: %s", self.__name, combination)
                logger.info(self)
                return combination
            except StopIteration:
                logger.info("%s no new combination", self.__name)
                logger.info(self)
                return None

    def done(self, combination):
        """mark the given element *done*"""
        with self.__lock:
            self.__done.add(combination)
            self.__inprogress.discard(combination)
            with open(self.__done_file, "w") as done_file:
                cPickle.dump(self.__done, done_file)
            self.__update_remaining()
            logger.info("%s combination done: %s", self.__name, combination)
            logger.info(self)

    def skip(self, combination):
        """mark the given element *skipped*"""
        with self.__lock:
            self.__skipped.add(combination)
            self.__inprogress.discard(combination)
            self.__update_remaining()
            logger.info("%s combination skipped: %s", self.__name, combination)
            logger.info(self)

    def reset(self):
        """reset container: iteration will start from beginning, state *skipped* are forgotten, state *done* are **not** forgotten"""
        with self.__lock:
            self.__inprogress.clear()
            self.__skipped.clear()
            self.__update_remaining()
            logger.info("%s reset", self.__name)
            logger.info(self)

    def num_total(self):
        """returns the total number of elements"""
        return len(self.__sweeps)

    def num_skipped(self):
        """returns the current number of *skipped* elements"""
        return len(self.__skipped)

    def num_remaining(self):
        """returns the current number of *todo* elements"""
        return len(self.__remaining)

    def num_inprogress(self):
        """returns the current number of elements which were obtained by a call to `execo_engine.utils.ParamSweeper.get_next`, not yet marked *done* or *skipped*"""
        return len(self.__inprogress)

    def num_done(self):
        """returns the current number of *done* elements"""
        return self.num_total() - (self.num_remaining() + self.num_inprogress() + self.num_skipped())
