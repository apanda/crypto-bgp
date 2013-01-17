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

from config import configuration
import logging
import sys

_ansi_styles = {
    'default'    : '\033[m',
    # styles
    'bold'       : '\033[1m',
    'underline'  : '\033[4m',
    'blink'      : '\033[5m',
    'reverse'    : '\033[7m',
    'concealed'  : '\033[8m',
    # font colors
    'black'      : '\033[30m',
    'red'        : '\033[31m',
    'green'      : '\033[32m',
    'yellow'     : '\033[33m',
    'blue'       : '\033[34m',
    'magenta'    : '\033[35m',
    'cyan'       : '\033[36m',
    'white'      : '\033[37m',
    # background colors
    'on_black'   : '\033[40m', 
    'on_red'     : '\033[41m',
    'on_green'   : '\033[42m',
    'on_yellow'  : '\033[43m',
    'on_blue'    : '\033[44m',
    'on_magenta' : '\033[45m',
    'on_cyan'    : '\033[46m',
    'on_white'   : '\033[47m',
    }
"""Definition of ansi escape sequences for colorized output."""

def set_style(string, ansi_style):
    """Enclose a string with ansi color escape codes if ``execo.config.configuration['color_mode']`` is True.

    :param string: the string to enclose
    
    :param ansi_style: an iterable of ansi attributes identifiers
      (those found in `execo.log._ansi_styles`)
    """
    ansi_style = 'style_' + ansi_style
    if (configuration.get('color_mode')
        and configuration.get(ansi_style)):
        style_seq = ""
        for attr in configuration[ansi_style]:
            style_seq += _ansi_styles[attr]
        return "%s%s%s" % (style_seq, string, _ansi_styles['default'])
    else:
        return string

# logger is the execo logging object
logger = logging.getLogger("execo")
"""The execo logger."""
logger_handler = logging.StreamHandler(sys.stdout)
logger_handler.setFormatter(logging.Formatter(set_style("%(asctime)s", 'log_header') + set_style(" %(name)s/%(levelname)s", 'log_level') + " %(message)s"))
logger.addHandler(logger_handler)
logger.setLevel(configuration.get('log_level'))
