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

"""Handles launch and control of several OS level processes concurrently.

Handles remote executions and file copies with ssh or similar tools.
"""

from log import logger
from config import configuration, default_connexion_params
from time_utils import sleep, Timer, format_date, format_duration, \
  get_seconds, get_unixts
from host import Host
from process import Process, SshProcess, get_process
from action import Action, wait_multiple_actions, wait_all_actions, \
  Remote, Put, Get, TaktukRemote, TaktukPut, TaktukGet, Local, \
  ParallelActions, SequentialActions, get_remote, get_fileput, get_fileget
from report import Report
