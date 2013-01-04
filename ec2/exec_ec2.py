#!/usr/bin/python

import os
import sys
import time

import execo
import boto
import pickle

import subprocess

from execo import Host
from execo import Remote
from execo import logging
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

instances_file = "instances.dat"
#INSTANCES_TYPE = 'm1.large'
INSTANCES_TYPE = 'm1.large'

def loadInstances(connection):
    instances = []
    
    file = open(instances_file, "r")
    ids = file.readlines()
    for id in ids:
        id = id.rstrip()
        instance = Instance(connection)
        instance.id = id
        instance.update()
        
        instances.append(instance)
    
    return instances



def dumpInstances(instances, append = False):
    if append: file = open(instances_file, "a")
    else: file = open(instances_file, "w")
    for instance in instances:
        file.write(instance.id + '\n')


def startInstances(conn, numOfInstances, oneRound = True):
    image = conn.get_all_images(['ami-27c90b4e']).pop()
 
    settleInterval = 40
    runningInstances = []
    
    while (True):
 
        pendingInstances = launchInstances(
          image, numOfInstances, runningInstances)
        print "Letting instances to settle down... please wait..."
        time.sleep(settleInterval)
        
        faultyInstances = verifyInstances(runningInstances)
        dumpInstances(runningInstances, True)
        runningInstances = []

        numOfInstances = len(faultyInstances) + len(pendingInstances)
        print 'There are total of %d faulty instances!' % numOfInstances
        terminateInstances(faultyInstances + pendingInstances)
        
        if numOfInstances == 0: break
        
        if(oneRound):
        	print "Running only one round finishing early"
        	break;
    
    return runningInstances


def startNoVerify(conn, numOfInstances):
	image = conn.get_all_images(['ami-27c90b4e']).pop()
	runningInstances = []
	pendingInstances = launchInstances(image, numOfInstances, runningInstances)
	dumpInstances(runningInstances, True)
        print 'Started',numOfInstances-len(pendingInstances),'(',len(pendingInstances), 'remaining pending)'
	terminateInstances(pendingInstances);
	
	return runningInstances


def launchInstances(image, imageNum, runningInstances):
    reservation = image.run(
        min_count = imageNum,
        max_count = imageNum,
        key_name = 'vjeko',
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
        if timeout > 20: break

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
    timeout = 5
        
    for instance in instances:
        hosts.append(Host(instance.public_dns_name))

    remote = Remote(hosts, command)
    remote.start()
    print "Verifying instances... please wait..."
    remote.wait(timeout)
    
    sortedInstances = sorted(instances, key=lambda inst: inst.public_dns_name)
    sortedProcesses = sorted(remote.processes_hosts(), key=lambda process: process.host().address)
    
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
    #os.system("ssh "+instance.public_dns_name+" \""+command+"\"");

    remote = Remote(hosts, command)
    remote.start()
    if async: return remote
    
    remote.wait()
    if remote.finished_ok():
        print 'All commands have executed successfully.'
    else:
        print 'Errors occurred.'
    
    return remote



def copy_out_file(instances,file_in, file_out):
	copy_out_file_range(instances,0, len(instances),file_in,file_out)



def copy_out_file_range(instances, start_id, end_id, file_in, file_out):
	processes = []
	for i,instance in enumerate(instances):
		if(i >= start_id and i <= end_id):
			comm = "scp -r "+file_in+" "+instance.public_dns_name+":"+file_out
			print comm
			#os.system(comm);
			p = subprocess.Popen(['scp','-r',file_in,instance.public_dns_name+":"+file_out]);
			processes.append(p)
	for proc in processes:
		proc.wait();



def copy_in(instances, remote_file, localdir):
	copy_in_range(instances, 0, len(instances), remote_file, localdir);



def copy_in_range(instances, start_id, end_id, remote_file, localdir):
	os.system('mkdir '+localdir);
	tokens = remote_file.split('/');
	if(len(tokens[len(tokens)-1]) == 0):
		pos_name = len(tokens)-2;
	else:
		pos_name = len(tokens)-1;
	remote_file_name = tokens[pos_name];
	print "remote file name: "+remote_file_name
	processes = []
	for i,instance in enumerate(instances):
        	if(i >= start_id and i <= end_id):
			comm = "scp -r "+instance.public_dns_name+":"+remote_file +" ./"+localdir+"/"+instance.private_ip_address+"_"+remote_file_name
			print comm
			#os.system(comm);
			p = subprocess.Popen(['scp','-r',instance.public_dns_name+":"+remote_file,"./"+localdir+"/"+instance.private_ip_address+"_"+remote_file_name]);
			processes.append(p);
	for proc in processes:
		proc.wait();


def main():
    
    if len(sys.argv) < 2:
        sys.exit("Need to specify an action.")
    
    elif sys.argv[1] == 'start':
        if len(sys.argv) < 3: sys.exit("Need to specify the number of hosts.")
        instanceNum = int(sys.argv[2])
        print 'Starting %d instances.' % instanceNum
        dumpInstances([])
        runningInstances = startInstances(conn, instanceNum)
        sys.exit()

    elif sys.argv[1] == 'start-one-round':
        if len(sys.argv) < 3: sys.exit("Need to specify the number of hosts.")
        instanceNum = int(sys.argv[2])
        print 'Attempting to start %d instances.' % instanceNum
        dumpInstances([])
        runningInstances = startInstances(conn, instanceNum, True)
        sys.exit()

    elif sys.argv[1] == 'start-no-verify':
        if len(sys.argv) < 3: sys.exit("Need to specify the number of hosts.")
        instanceNum = int(sys.argv[2])
        print 'Attempting to start %d instances.' % instanceNum
        dumpInstances([])
        runningInstances = startNoVerify(conn, instanceNum)
        sys.exit()


    elif sys.argv[1] == 'add':
        if len(sys.argv) < 3: sys.exit("Need to specify the number of hosts.")
        instanceNum = int(sys.argv[2])
        print 'Starting %d instances.' % instanceNum
        runningInstances = startInstances(conn, instanceNum)
        dumpInstances(runningInstances, True)
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

    elif sys.argv[1] == 'sync-exec-all':
        if len(sys.argv) < 3: sys.exit("Need to specify a command.")
        command = sys.argv[2].replace('\'', '')
        print 'Executing command: %s' % command
        runningInstances = loadInstances(conn)
        remote = execCommand(runningInstances, command, False)
        for process in remote.processes():
            print process.stdout()

    elif sys.argv[1] == 'exec-range':
        if len(sys.argv) < 5: sys.exit("Usage: "+sys.argv[0]+" exec-range startno endno command");
        command = sys.argv[4].replace('\'', '')
        print 'Executing command: %s' % command
        runningInstances = loadInstances(conn)
        remote = execCommandRange(runningInstances, command, int(sys.argv[2]),int(sys.argv[3]))
            
    elif sys.argv[1] == 'start-ovs':
        print 'Starting Open VSwitch.'
        command = 'sudo service ovs start'
        runningInstances = loadInstances(conn)
        remote = execCommand(runningInstances, command)
        
    elif sys.argv[1] == 'start-nox':
        print 'Starting CP NOX.'
        #command = 'sudo service ovs-nox start'
        command = 'cd /opt/nox.cloudpolice/src/; sudo ./.libs/lt-nox_core -i ptcp: cloudpolice > output_nox 2>&1'
        runningInstances = loadInstances(conn)
        remote = execCommand(runningInstances, command)

    elif sys.argv[1] == 'start-nox-debug':
        print 'Starting CP NOX.'
        #command = 'sudo service ovs-nox start'
        command = 'cd /opt/nox.cloudpolice/src/; sudo ./.libs/lt-nox_core -v -i ptcp: cloudpolice > output_nox 2>&1'
        runningInstances = loadInstances(conn)
        remote = execCommand(runningInstances, command)

    elif sys.argv[1] == 'kill-nox':
        print 'killng NOX'
        command = 'sudo killall lt-nox_core'
        runningInstances = loadInstances(conn)
        remote = execCommand(runningInstances, command)

    
    elif sys.argv[1] == 'copy-out-all':
       	if(len(sys.argv) < 4): sys.exit("Usage: "+sys.argv[0]+" copy-out-all file-in file-remote");
      	print 'Copying file ',sys.argv[2],'into',sys.argv[3]
      	runningInstances = loadInstances(conn)
      	copy_out_file(runningInstances,sys.argv[2],sys.argv[3]);

    elif sys.argv[1] == 'copy-out-range':
      	if(len(sys.argv) < 6): sys.exit("Usage: "+sys.argv[0]+" copy-out-range file-in file-remote");
      	print 'Copying file ',sys.argv[4],'into',sys.argv[5]
       	runningInstances = loadInstances(conn)
       	copy_out_file_range(runningInstances, int(sys.argv[2]),int(sys.argv[3]), sys.argv[4], sys.argv[5]);

    elif sys.argv[1] == 'copy-in-all':
        if(len(sys.argv) < 4): sys.exit("Usage: "+sys.argv[0]+" copy-in-all file-remote local-dir");
        runningInstances = loadInstances(conn)
        copy_in(runningInstances, sys.argv[2], sys.argv[3]);

    elif sys.argv[1] == 'copy-in-range':
        if(len(sys.argv) < 6): sys.exit("Usage: "+sys.argv[0]+" copy-in-range start_id end_id file-remote local-dir");
        runningInstances = loadInstances(conn)
        copy_in_range(runningInstances, int(sys.argv[2]), int(sys.argv[3]), sys.argv[4], sys.argv[5]);

    else:
    	  print "ERROR: unknown command:",sys.argv[1];
    	  sys.exit();
    
    print "Process has ended."



if __name__ == "__main__":
    main()
