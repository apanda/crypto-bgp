
********************************
At this point in the directory structure (above the IDR[1|2|3] folders) there
needs to be:
    sepia.jar
    idr.jar
    daemon.jar
	
To kick everything off:
    $ ./runAll.sh

********************************

To add more nodes, copy one of the old IDR instances and create one instance for
each new node (following the IDR# naming). The following files need to be edited:

runAll.sh
	NODES=
		needs to be the number of nodes
test.txt
	should have the updated topology
peer*/input/test.txt
	should have the updated preference lists / export tables for all nodes
	handled by this inputpeer
privacypeer*/config.privacypeer*.properties:
	mpc.idr.V=
		should be set to number of nodes; right now this is not required

In the new instance(s):

peer*/config.peer*.properties:
    peers.activeprivacypeers=
		needs unique host:port for each privacy peer in this instance
	
privacypeer*/config.privacypeer*.properties:
	peers.activeprivacypeers=
		needs to have host:port for each privacy peer in this instance
    mpc.idr.nodeid=
		needs to be the ID number of the node for IDR instance
		
		