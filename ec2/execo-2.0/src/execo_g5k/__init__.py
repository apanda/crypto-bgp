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

"""Execo extension and tools for Grid5000."""

from config import g5k_configuration, default_frontend_connexion_params, \
  default_oarsh_oarcp_params
from oar import OarSubmission, oarsub, oardel, get_current_oar_jobs, \
  get_oar_job_info, wait_oar_job_start, get_oar_job_nodes, \
  get_oar_job_subnets
from oargrid import oargridsub, oargriddel, get_current_oargrid_jobs, \
  get_oargrid_job_info, get_oargrid_job_oar_jobs, wait_oargrid_job_start, \
  get_oargrid_job_nodes
from kadeploy import Deployment, Kadeployer, kadeploy, deploy
from utils import g5k_host_get_cluster, g5k_host_get_site, group_hosts
try:
    from api_utils import get_g5k_sites, get_site_clusters, \
      get_cluster_hosts, get_g5k_clusters, get_g5k_hosts, get_cluster_site
except ImportError:
    # probably if httplib2 is not installed.
    pass
