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

from utils import comma_join

class Host(object):

    """A host to connect to.

    - Has an address (mandatory)

    - Can optionaly have a user, a keyfile, a port, which are used for
      remote connexion and authentification (with a ssh like remote
      connexion tool).

    Has an intuitive comparison and hashing behavior: two `execo.host.Host` with
    the same members (address, user, keyfile, port) will hash equally
    and will be seen as identical keys in a set or dict.

    >>> h1 = Host('localhost')
    >>> h1.user = 'root'
    >>> h1
    Host('localhost', user='root')
    >>> h2 = Host('localhost', user = 'root')
    >>> h1 == h2
    True
    >>> d = dict()
    >>> d[h1] = True
    >>> d[h2]
    True
    """

    def __init__(self, address, user = False, keyfile = False, port = False):
        """
        :param address: (string or `execo.host.Host`) the host address
          or another `execo.host.Host` instance which will be copied
          into this new instance

        :param user: (string) optional user whith which to connect. If
          False (default value), means use the default user. If None,
          means don't use any user.

        :param keyfile: (string) optional keyfile whith which to
          connect. If False (default value), means use the default
          keyfile. If None, means don't use any keyfile.

        :param port: (integer) optional port to which to connect. If
          False (default value), means use the default port. If None,
          means don't use any port.
        """
        if isinstance(address, Host):
            self.address = address.address
            self.user = address.user
            self.keyfile = address.keyfile
            self.port = address.port
        else:
            self.address = address
            self.user = None
            self.keyfile = None
            self.port = None
        if user != False: self.user = user
        if keyfile != False: self.keyfile = keyfile
        if port != False: self.port = port

    def __eq__(self, other):
        if not other or not isinstance(other, Host):
            return False
        return (self.address == other.address and
                self.user == other.user and
                self.keyfile == other.keyfile and
                self.port == other.port)

    def __ne__(self, other):
        return not self.__eq__(other)

    def __hash__(self):
        return (self.address.__hash__()
                ^ self.user.__hash__()
                ^ self.keyfile.__hash__()
                ^ self.port.__hash__())

    def _args(self):
        args = "%r" % (self.address,)
        if self.user: args = comma_join(args, "user=%r" % (self.user,))
        if self.keyfile: args = comma_join(args, "keyfile=%r" % (self.keyfile,))
        if self.port: args = comma_join(args, "port=%r" % (self.port,))
        return args

    def __repr__(self):
        return "Host(%s)" % (self._args())

def get_hosts_list(hosts):
    """Deep copy an iterable of `execo.host.Host` to a list of `execo.host.Host`.

    order is preserved
    """
    return [ Host(host) for host in hosts ]

def get_hosts_set(hosts):
    """Deep copy an iterable of `execo.host.Host` to a set of `execo.host.Host`."""
    return set(get_hosts_list(hosts))
