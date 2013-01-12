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
import datetime
import re
import time

_epoch = datetime.datetime (1970, 1, 1, 0, 0, 0, 0)

def timedelta_to_seconds(td):
    """Convert a ``datetime.timedelta`` to a number of seconds (float)."""
    return td.days * 86400 + td.seconds + td.microseconds / 1e6

def datetime_to_unixts(dt):
    """Convert a ``datetime.datetime`` to a unix timestamp (float)."""
    elapsed = dt - _epoch
    return timedelta_to_seconds(elapsed)

def unixts_to_datetime(ts):
    """Convert a unixts (int or float) to ``datetime.datetime``."""
    days = int(ts) / 86400
    seconds = int(ts) % 86400
    elapsed = datetime.timedelta(days, seconds)
    return _epoch + elapsed

def numeric_date_to_unixts(d):
    """Convert a numeric to unix timestamp (float)."""
    return float(d)

def numeric_duration_to_seconds(d):
    """Convert a numeric to duration in number of seconds (float)."""
    return float(d)

_num_str_date_re = re.compile("^((\d*\.)?\d+)$")
_str_date_re = re.compile("^(\d\d\d\d-\d\d-\d\d \d\d:\d\d:\d\d)(\.\d+)?$")
def str_date_to_unixts(d):
    """Convert a date string to a unix timestamp (float).

    date string format can be:

    - a numeric. In this case, convert with
      `execo.time_utils.numeric_date_to_unixts`

    - a string in format 'YYYY-MM-DD HH:MM:SS[.MS]'
    """
    numeric_date = _num_str_date_re.match(d)
    if numeric_date:
        return numeric_date_to_unixts(float(numeric_date.group(1)))
    str_date = _str_date_re.match(d)
    if str_date:
        ts = time.mktime(time.strptime(str_date.group(1), "%Y-%m-%d %H:%M:%S"))
        if str_date.group(2):
            ts += float(str_date.group(2))
        return ts
    raise ValueError, "unsupported date format %s" % (d,)

_num_str_duration_re = re.compile("^((\d*\.)?\d+)$")
_str_duration_re = re.compile("^(\d+):(\d?\d):(\d?\d)(\.\d+)?$")
def str_duration_to_seconds(duration):
    """Convert a duration string to a number of seconds (float).

    duration string format can be:

    - a numeric. In this case, convert with
      `execo.time_utils.numeric_duration_to_seconds`

    - a string in format 'HH:MM:SS[.MS]'
    """
    numeric_duration = _num_str_duration_re.match(duration)
    if numeric_duration:
        return numeric_duration_to_seconds(float(numeric_duration.group(1)))
    str_duration = _str_duration_re.match(duration)
    if str_duration:
        duration = (int(str_duration.group(1)) * 3600
                    + int(str_duration.group(2)) * 60
                    + int(str_duration.group(3)))
        if str_duration.group(4):
            duration += float(str_duration.group(4))
        return duration
    raise ValueError, "unsupported duration format %s" % (duration,)

def get_seconds(duration):
    """Convert a duration to a number of seconds (float).

    :param duration: a duration in one of the supported types. if
      duration == None, returns None. Supported types

      - ``datetime.timedelta``

      - string: see `execo.time_utils.str_duration_to_seconds`

      - numeric type: see
        `execo.time_utils.numeric_duration_to_seconds`
    """
    if duration == None:
        return None
    elif isinstance(duration, datetime.timedelta):
        return timedelta_to_seconds(duration)
    elif isinstance(duration, str):
        return str_duration_to_seconds(duration)
    return numeric_duration_to_seconds(duration)

def get_unixts(d):
    """Convert a date to a unix timestamp (float).

    :param d: a date in one of the supported types. if date == None,
      returns None. Supported types

      - ``datetime.datetime``

      - string: see `execo.time_utils.str_date_to_unixts`

      - numeric type: see `execo.time_utils.numeric_date_to_unixts`
    """
    if d == None:
        return None
    if isinstance(d, datetime.datetime):
        return datetime_to_unixts(d)
    elif isinstance(d, str):
        return str_date_to_unixts(d)
    return numeric_date_to_unixts(d)

def _get_milliseconds_suffix(secs):
    """Return a formatted millisecond suffix, either empty if ms = 0, or dot with 3 digits otherwise.

    :param secs: a unix timestamp (integer or float)
    """
    ms_suffix = ""
    msecs = int (round(secs - int(secs), 3) * 1000)
    if msecs != 0:
        ms_suffix = ".%03i" % msecs
    return ms_suffix

def _zone2822(timetuple):
    dst = timetuple[8]
    offs = (time.timezone, time.timezone, time.altzone)[1 + dst]
    return '%+.2d%.2d' % (offs / -3600, abs(offs / 60) % 60)

def format_unixts(secs, showms = False):
    """Return a string with the formatted date (year, month, day, hour, min, sec, ms) for pretty printing.

    :param secs: a unix timestamp (integer or float) (or None).

    :param showms: whether to show ms or not. Default False.
    """
    if secs == None:
        return None
    t = time.localtime(secs)
    formatted_time = time.strftime("%Y-%m-%d %H:%M:%S", t)
    if showms:
        formatted_time += _get_milliseconds_suffix(secs)
    formatted_time += " " + _zone2822(t)
    return formatted_time

def format_seconds(secs, showms = False):
    """Return a string with a formatted duration (days, hours, mins, secs, ms) for pretty printing.

    :param secs: a duration in seconds (integer or float) (or None).

    :param showms: whether to show ms or not. Default False.
    """
    if secs == None:
        return None
    s = secs
    d = (s - (s % 86400)) / 86400
    s -= d * 86400
    h = (s - (s % 3600)) / 3600
    s -= h * 3600
    m = (s - (s % 60)) / 60
    s -= m * 60
    formatted_duration = ""
    if secs >= 86400: formatted_duration += "%id" % d
    if secs >= 3600: formatted_duration += "%ih" % h
    if secs >= 60: formatted_duration += "%im" % m
    if showms:
        formatted_duration += "%i%ss" % (s, _get_milliseconds_suffix(s))
    else:
        formatted_duration += "%is" % (s,)
    return formatted_duration

def format_date(d, showms = False):
    """Return a string with the formatted date (year, month, day, hour, min, sec, ms) for pretty printing.

    :param d: a date in one of the formats handled (or None) (see
      `execo.time_utils.get_unixts`).

    :param showms: whether to show ms or not. Default False.
    """
    return format_unixts(get_unixts(d), showms)

def format_duration(duration, showms = False):
    """Return a string with a formatted duration (days, hours, mins, secs, ms) for pretty printing.

    :param duration: a duration in one of the formats handled (or
      None) (see `execo.time_utils.get_seconds`).

    :param showms: whether to show ms or not. Default False.
    """
    return format_seconds(get_seconds(duration), showms)

def _safe_sleep(secs):
    """Safe sleeping: restarted if interrupted by signals.

    :param secs: time to sleep in seconds (int or float)
    """
    end = time.time() + secs
    sleep_time = secs
    while sleep_time > 0:
        time.sleep(sleep_time)
        sleep_time = end - time.time()

def sleep(delay = None, until = None):
    """Sleep until a given delay has elapsed or until a given date.

    If both present, will sleep at least for the delay and at least
    until the date.

    :param delay: the delay to sleep in one of the formats handled (or
      None) (see `execo.time_utils.get_seconds`).

    :param until: the date until which to sleep in one of the formats
      handled (or None) (see `execo.time_utils.get_unixts`).
    """
    delay = get_seconds(delay)
    until = get_unixts(until)
    sleeptime = 0
    if delay != None:
        sleeptime = delay
    if until != None:
        dt = until - time.time()
        if (sleeptime > 0 and dt > sleeptime) or (sleeptime <= 0 and dt > 0):
            sleeptime = dt
    if sleeptime > 0:
        logger.info("sleeping %s", format_seconds(sleeptime))
        _safe_sleep(sleeptime)
        logger.info("end sleeping")
        return sleeptime

class Timer(object):
    
    """Keep track of elapsed time."""
    
    def __init__(self, timeout = None):
        """Create and start the Timer."""
        self._start = time.time()
        self._timeout = get_seconds(timeout)

    def wait_elapsed(self, elapsed):
        """Sleep until the given amount of time has elapsed since the Timer's start.

        :param elapsed: the delay to sleep in one of the formats
          handled (or None) (see `execo.time_utils.get_seconds`).
        """
        elapsed = get_seconds(elapsed)
        really_elapsed = time.time() - self._start
        if really_elapsed < elapsed:
            sleep(elapsed - really_elapsed)
        return self

    def start_date(self):
        """Return this Timer's instance start time."""
        return self._start

    def elapsed(self):
        """Return this Timer's instance elapsed time since start."""
        return time.time() - self._start

    def remaining(self):
        """Returns the remaining time before the timeout."""
        if self._timeout != None:
            return self._start + self._timeout - time.time()
        else:
            return None
