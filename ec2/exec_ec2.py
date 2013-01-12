#!/usr/bin/python

import os
import sys
import time

sys.path.append(os.path.join( 
  os.path.dirname(__file__), 
  'execo-2.0',
  'src' ))


import execo
import boto
import pickle

import subprocess

from execo import Host
from execo import Remote
from execo import SshProcess

from boto.ec2.connection import EC2Connection
from boto.ec2.connection import EC2ResponseError
from boto.ec2.image import Image
from boto.ec2.keypair import KeyPair
from boto.ec2.instance import Reservation
from boto.ec2.instance import Instance



#config
conn = EC2Connection(
        aws_access_key_id = 'AKIAIOPRNXFE5YO3VOWQ',
        aws_secret_access_key = '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')

AMI = 'ami-818a02e8'
INSTANCES_FILE = "instances.dat"
INSTANCES_TYPE = 'm1.xlarge'
SECURITY_GROUPS = ['vjeko']

def loadInstances(connection):
    instances = []
    
    file = open(INSTANCES_FILE, "r")
    ids = file.readlines()
    for id in ids:
        id = id.rstrip()
        instance = Instance(connection)
        instance.id = id
        instance.update()
        
        instances.append(instance)
    
    return instances



def dumpInstances(instances, append = False):
    if append: file = open(INSTANCES_FILE, "a")
    else: file = open(INSTANCES_FILE, "w")
    for instance in instances:
        file.write(instance.id + '\n')



def startInstances(conn, numOfInstances, oneRound = False):
    image = conn.get_all_images([AMI]).pop()
 
    settleInterval = 40
    runningInstances = []
    
    while (True):
 
        pendingInstances = launchInstances(
          image, numOfInstances, runningInstances)
        print "Letting instances to settle down... please wait..."
        time.sleep(settleInterval)
        
        faultyInstances = verifyInstances(runningInstances)
        dumpInstances(runningInstances, True)

        numOfInstances = len(faultyInstances) + len(pendingInstances)
        print 'There are total of %d faulty instances!' % numOfInstances
        terminateInstances(faultyInstances + pendingInstances)
        
        if numOfInstances == 0: break
        
        if(oneRound):
          print "Running only one round, finishing early."
          break;

    print runningInstances
    return runningInstances



def launchInstances(image, imageNum, runningInstances):
    reservation = image.run(
        min_count = imageNum,
        max_count = imageNum,
        key_name = 'vjeko',
        security_groups = SECURITY_GROUPS,
        instance_type = INSTANCES_TYPE)
    
    timeout = 0
    cSleepInterval = 1
    pendingInstances = reservation.instances
    
    while (True):
        for instance in pendingInstances:
            
            if instance.state == 'running':
                pendingInstances.remove(instance)
                runningInstances.append(instance)
            try: instance.update()
            except: print 'Unable to update...'
            
        time.sleep(cSleepInterval)
        timeout = timeout + cSleepInterval
        print 'There are %d instances pending... ' % len(pendingInstances)
        if len(pendingInstances) == 0: break
        if timeout > 40: break

    print "There are %d pending/faulty instances." % len(pendingInstances)
    return pendingInstances




def terminateInstances(runningInstances):
    for instance in runningInstances:
        print 'Terminating instance %s.' % instance.id
        instance.terminate()



def verifyInstances(instances):
    
    hosts = []
    faultyInstances = []
    command = 'uname -a'
    timeout = 20
        
    for instance in instances:
        hosts.append(Host(instance.public_dns_name))

    remote = Remote(command, hosts)
    remote.start()
    print "Verifying instances... please wait..."
    remote.wait(timeout)
    
    sortedInstances = sorted(instances, key=lambda inst: inst.public_dns_name)
    sortedProcesses = sorted(remote.processes(), key=lambda process: process.host().address)
    
    for index, handler in enumerate(sortedProcesses):
        instance = sortedInstances[index]
        if not handler.ok() or not handler.ended():
            print 'Instance %s is faulty!' % instance.public_dns_name
            faultyInstances.append(instance)
        else:
            print 'Instance %s OK!' % instance.public_dns_name
    
    for instance in faultyInstances:
        instances.remove(instance)
    
    return faultyInstances



def execCommand(instances, command, async = True):
    return execCommandRange(instances, command, 0, len(instances), async);



def execCommandRange(instances, command, startid, endid, async = True):
    hosts = []        
    for i,instance in enumerate(instances):
      if(i >= startid and i <= endid):
          hosts.append(Host(instance.public_dns_name))

    remote = Remote(command, hosts)
    remote.start()
    if async: return remote
    
    remote.wait()
    if remote.finished_ok():
        print 'All commands have executed successfully.'
    else:
        print 'Errors occurred.'
    
    return remote



def main():
    
    if len(sys.argv) < 2:
        sys.exit("Need to specify an action.")
    
    elif sys.argv[1] == 'start':
        if len(sys.argv) < 3: sys.exit("Need to specify the number of hosts.")
        instanceNum = int(sys.argv[2])
        print 'Starting %d instances.' % instanceNum
        runningInstances = startInstances(conn, instanceNum)
        dumpInstances(runningInstances)
        sys.exit()

    elif sys.argv[1] == 'start-one-round':
        if len(sys.argv) < 3: sys.exit("Need to specify the number of hosts.")
        instanceNum = int(sys.argv[2])
        print 'Attempting to start %d instances.' % instanceNum
        dumpInstances([])
        runningInstances = startInstances(conn, instanceNum, True)
        sys.exit()

    elif sys.argv[1] == 'add':
        if len(sys.argv) < 3: sys.exit("Need to specify the number of hosts.")
        instanceNum = int(sys.argv[2])
        print 'Starting %d instances.' % instanceNum
        runningInstances = startInstances(conn, instanceNum)
        sys.exit()

    elif sys.argv[1] == 'verify':
        instances = loadInstances(conn)
        faultyInstances = verifyInstances(instances)
        sys.exit()
        
    elif sys.argv[1] == 'verify-and-terminate':
        instances = loadInstances(conn)
        faultyInstances = verifyInstances(instances)
        terminateInstances(faultyInstances)
        dumpInstances(instances)
        sys.exit()
        
    elif sys.argv[1] == 'info':
        runningInstances = loadInstances(conn)
        for instance in runningInstances:
            print 'Instance %s:' % instance.id
            print '\tState:       %s' % instance.state
            print '\tPublic DNS:  %s' % instance.public_dns_name
            print '\tPrivate DNS: %s' % instance.private_dns_name
            print '\tPrivate IP:  %s' % instance.private_ip_address
        sys.exit()
        
    elif sys.argv[1] == 'terminate':
        runningInstances = loadInstances(conn)
        terminateInstances(runningInstances)
        sys.exit()
        
    elif sys.argv[1] == 'exec-all':
        if len(sys.argv) < 3: sys.exit("Need to specify a command.")
        command = sys.argv[2].replace('\'', '')
        print 'Executing command: %s' % command
        runningInstances = loadInstances(conn)
        remote = execCommand(runningInstances, command)

    else:
    	  print "ERROR: unknown command:",sys.argv[1];
    	  sys.exit();
    
    print "Process has ended."



if __name__ == "__main__":
    main()
