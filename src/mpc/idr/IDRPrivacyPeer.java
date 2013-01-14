// Copyright 2010-2012 Martin Burkhart (martibur@ethz.ch)
//
// This file is part of SEPIA. SEPIA is free software: you can redistribute 
// it and/or modify it under the terms of the GNU Lesser General Public 
// License as published by the Free Software Foundation, either version 3 
// of the License, or (at your option) any later version.
//
// SEPIA is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with SEPIA.  If not, see <http://www.gnu.org/licenses/>.

package mpc.idr;

import java.io.FileInputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.net.Socket;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Observable;
import java.util.Properties;
import java.util.Random;
import java.util.Vector;
import java.util.logging.Level;

import mpc.CountingBarrier;
import mpc.VectorData;
import mpc.protocolPrimitives.Primitives;
import mpc.protocolPrimitives.PrimitivesEnabledProtocol;
import services.Services;
import services.Stopper;
import startup.ConfigFile;
import connections.ConnectionManager;
import events.FinalResultEvent;

/**
 * A MPC privacy peer with the computation capabilities for the tutorial protocol
 * 
 * @author Dilip Many
 *
 */
public class IDRPrivacyPeer extends IDRBase {

	public static final String PROP_IDR_V = "mpc.idr.V"; // number of nodes in graph
	public static final String PROP_IDR_NODE_ID = "mpc.idr.nodeid"; // ID of the node this privacy peer is for
	public static final String PROP_IDR_DAEMON_PORT = "mpc.idr.dport"; // port to connect to daemon
	public static final String PROP_IDR_DAEMON_HOST = "mpc.idr.dhost"; // defaults to localhost
	public static final String PROP_IDR_FAKE_DAEMON = "mpc.idr.fake"; // true if daemon results should be faked 
	public static final String PROP_N_ITEMS = "nitems";

	private long nodeID; // the ID of the only node this cares about

	protected boolean fakeDaemon;

	/** vector of protocols (between this privacy peer and the peers) */
	private Vector<IDRProtocolPrivacyPeerToPeer> peerProtocolThreads = null;
	/** vector of protocols (between this privacy peer and other privacy peers) */
	private Vector<IDRProtocolPrivacyPeerToPP> ppToPPProtocolThreads = null;
	/** vector of information objects for the connected peers */
	protected Vector<IDRPeerInfo> peerInfos = null;
	/** vector of information objects for the connected privacy peers */
	protected Vector<IDRPeerInfo> privacyPeerInfos = null;
	/** barrier to synchronize the peerProtocolThreads threads */
	private CountingBarrier peerProtocolBarrier = null;
	/** barrier to synchronize the ppToPPProtocolThreads threads */
	private CountingBarrier ppProtocolBarrier = null;

	/** number of input peers connected to this one */
	protected int numberOfInputPeers = 0;
	/** number of initial shares that the privacy peer yet has to receive */
	private int initialSharesToReceive = 0;

	/** expected number of nodes = V */
	private int numberOfNodes = 0;

	/** contains the info for all nodes */
	protected Vector<IDRNodeInfo> nodeInfos = null;

	/** contains the adjacency list for all nodes */
	private Vector<long[]> topology;

	private Properties properties;
	private boolean readOperationSuccessful;

	private Socket daemonSock;
	private ObjectOutputStream outStream;// = new ObjectOutputStream(s.getOutputStream());
	private ObjectInputStream inStream;// = new ObjectInputStream(s.getInputStream());

	//private long [] useHop;
	//private long [] dontUseHop;

	private long[] route = null;
	private long pathLength = -1;

	private Random rand = null;

	/**
	 * creates a new MPC tutorial privacy peer
	 *
	 * @param myPeerIndex	This peer's number/index
	 * @param stopper		Stopper (can be used to stop this thread)
	 * @param cm 			the connection manager
	 * @throws Exception
	 */
	public IDRPrivacyPeer(int myPeerIndex, ConnectionManager cm, Stopper stopper) throws Exception {
		super(myPeerIndex, cm, stopper);

		peerInfos = new Vector<IDRPeerInfo>();
		privacyPeerInfos = new Vector<IDRPeerInfo>();
		peerProtocolThreads = new Vector<IDRProtocolPrivacyPeerToPeer>();
		ppToPPProtocolThreads = new Vector<IDRProtocolPrivacyPeerToPP>();
	}

	@Override
	protected synchronized void initProperties() throws Exception {
		super.initProperties();

		Properties properties = ConfigFile.getInstance().getProperties();
		// Set properties specific to IDR privacy peers; note, this is optional
		numberOfNodes = Integer.parseInt(properties.getProperty(PROP_IDR_V, "1"));
		logger.log(Level.INFO, "IDR expected number of nodes ="+numberOfNodes);

		nodeID = Long.parseLong(properties.getProperty(PROP_IDR_NODE_ID));

		fakeDaemon = properties.getProperty(PROP_IDR_FAKE_DAEMON, "false").equalsIgnoreCase("true");
		try {
			boolean b = Integer.parseInt(properties.getProperty(PROP_IDR_FAKE_DAEMON, "0")) == 1;
			fakeDaemon = fakeDaemon || b;
		} catch (NumberFormatException e) {}

		if (fakeDaemon) {
			logger.log(Level.WARNING, "Not connecting to daemon; results will be incorrect");
		} else { // !fakeDaemon
			int port = Integer.parseInt(properties.getProperty(PROP_IDR_DAEMON_PORT));
			String host = properties.getProperty(PROP_IDR_DAEMON_HOST, "localhost");
			connectDaemon(host, port);
		}

	}

	private void connectDaemon(String host, int port) throws IOException {
		try {
			daemonSock = new Socket(host, port);
			outStream = new ObjectOutputStream(daemonSock.getOutputStream());
			inStream = new ObjectInputStream(daemonSock.getInputStream());
			outStream.writeInt((int)nodeID);
			outStream.flush();
		} catch (UnknownHostException e) {
			logger.severe("Could not determine IP address of daemon!");
			throw e;
		} catch (IOException e) {
			logger.severe("Unable to open socket to daemon!");
			throw e;
		}
	}

	/**
	 * Initializes the privacy peer
	 */
	public void initialize() throws Exception {
		initProperties();

		currentTimeSlot = 1;
	}


	/**
	 * Initializes a new round of computation.
	 */
	protected void initializeNewRound()  {
		connectionManager.waitForConnections();
		connectionManager.activateTemporaryConnections();
		PrimitivesEnabledProtocol.newStatisticsRound();

		// Get all the active privacy peer IDs. Note that these are not necessarily all PPs configured in the config file.
		List<String> privacyPeerIDs = connectionManager.getActivePeers(true);
		List<String> inputPeerIDs = connectionManager.getActivePeers(false);
		Map<String, Integer> ppIndexMap = getIndexMap(privacyPeerIDs);
		myAlphaIndex = ppIndexMap.get(myPeerID);

		numberOfPrivacyPeers = privacyPeerIDs.size()+1; // Count myself
		numberOfInputPeers = inputPeerIDs.size();
		peerProtocolBarrier = new CountingBarrier(numberOfInputPeers);
		ppProtocolBarrier = new CountingBarrier(numberOfPrivacyPeers-1);
		clearPP2PPBarrier();

		// init counters
		initialSharesToReceive = numberOfInputPeers;
		finalResultsToDo = numberOfInputPeers;
		finalResult = null;

		// init NodeInfos and topology
		nodeInfos = new Vector<IDRNodeInfo>(numberOfNodes);
		topology = new Vector<long[]>(numberOfNodes);
		for (int i = 0; i < numberOfNodes; i++) {
			nodeInfos.add(null);
			topology.add(new long[]{});
		}

		// read topology file
		readTopology(topo_filename);
		if (!readOperationSuccessful) {
			logger.severe("Could not read topology from " + topo_filename + " -- returning");
			return;
		}

		primitives = new Primitives(randomAlgorithm, shamirSharesFieldOrder, degreeT, numberOfPrivacyPeers, myAlphaIndex, numberOfPrivacyPeers-1);
		createProtocolThreadsForInputPeers(inputPeerIDs);
		createProtocolThreadsForPrivacyPeers(privacyPeerIDs, ppIndexMap);
	}

	/**
	 * Generates a consistent mapping from active privacy peer IDs to privacy peer indices.
	 * @param connectedPrivacyPeerIDs all connected PPs, without myself.
	 * @return the index map
	 */
	private Map<String,Integer> getIndexMap(List<String> connectedPrivacyPeerIDs) {
		List<String> allPPsorted = new ArrayList<String>();
		allPPsorted.addAll(connectedPrivacyPeerIDs);
		allPPsorted.add(getMyPeerID());

		Collections.sort(allPPsorted);
		HashMap<String,Integer> indexMap = new HashMap<String, Integer>();
		for(int index=0; index<allPPsorted.size(); index++) {
			indexMap.put(allPPsorted.get(index), index);
		}
		return indexMap;
	}

	/**
	 * Create and start the threads. Attach one privacy peer id to each of them.
	 * 
	 * @param privacyPeerIDs
	 *            the ids of the privacy peers
	 * @param ppIndexMap
	 * 			  a map mapping privacy peer IDs to indices
	 */
	private void createProtocolThreadsForPrivacyPeers(List<String> privacyPeerIDs, Map<String, Integer> ppIndexMap) {
		ppToPPProtocolThreads.clear();
		privacyPeerInfos.clear();
		int currentID =0;
		for(String ppId: privacyPeerIDs) {
			logger.log(Level.INFO, "Create a thread for privacy peer " +ppId );
			int otherPPindex = ppIndexMap.get(ppId);
			IDRProtocolPrivacyPeerToPP pp2pp = new IDRProtocolPrivacyPeerToPP(currentID, this, ppId, otherPPindex, stopper);
			pp2pp.setMyPeerIndex(myAlphaIndex);
			pp2pp.addObserver(this);
			Thread thread = new Thread(pp2pp, "IDR PP-to-PP protocol connected with " + ppId);
			ppToPPProtocolThreads.add(pp2pp);
			privacyPeerInfos.add(currentID, new IDRPeerInfo(ppId, otherPPindex));
			thread.start();
			currentID++;
		}
	}

	/**
	 * Create and start the threads. Attach one input peer id to each of them.
	 * 
	 * @param inputPeerIDs
	 *            the ids of the input peers
	 */
	private void createProtocolThreadsForInputPeers(List<String> inputPeerIDs) {
		peerProtocolThreads.clear();
		peerInfos.clear();
		int currentID = 0;
		for(String ipId: inputPeerIDs) {
			logger.log(Level.INFO, "Create a thread for input peer " +ipId );
			IDRProtocolPrivacyPeerToPeer pp2p = new IDRProtocolPrivacyPeerToPeer(currentID, this, ipId, currentID, stopper);
			pp2p.addObserver(this);
			Thread thread = new Thread(pp2p, "IDR Peer protocol connected with " + ipId);
			peerProtocolThreads.add(pp2p);
			peerInfos.add(currentID, new IDRPeerInfo(ipId, currentID));
			thread.start();
			currentID++;
		}
	}

	protected boolean readTopology(String filename) {
		readOperationSuccessful = false;
		try {
			properties = new Properties();
			FileInputStream in = new FileInputStream(filename);
			properties.load(in);
			in.close();
			int nItems = Integer.parseInt(properties.getProperty(PROP_N_ITEMS));
			/* int numNodes = 0;
			while (nItems > numNodes) {
				nodeInfos.add(null);
				topology.add(new long[]{});
                numNodes++;
			} */
			if (nItems != numberOfNodes) {
				return false;
			}
			for (int i = 0; i < nItems; i++) {
				String nodeName = (i+1) + "";
				String[] neighborStrings = properties.getProperty(nodeName,"").split("\\D");
				long[] neighbors = new long[neighborStrings.length];
				for (int j = 0; j < neighborStrings.length; j++) {
					try {
						neighbors[j] = Long.parseLong(neighborStrings[j]);
					} catch (NumberFormatException nfe) {
						neighbors[j] = -1L;
					}
				}
				topology.set(i, neighbors);
			}
			readOperationSuccessful = true;
		} catch (IOException e) {
			e.printStackTrace();
		}
		return readOperationSuccessful;
	}

	/**
	 * Run the MPC protocol(s) over the given connection(s).
	 */
	public synchronized void runProtocol() {
		// All we need to do here is starting the first round
		initializeNewRound();
	}

	/**
	 * Process message received by an observable.
	 * 
	 * @param observable	Observable who sent the notification
	 * @param object		The object that was sent by the observable
	 */
	protected void notificationReceived(Observable observable, Object object) throws Exception {
		if (object instanceof IDRMessage) {
			IDRMessage msg = (IDRMessage) object;
			// We are awaiting a message with initial shares 
			if (msg.isDummyMessage()) {
				// Counterpart is offline. Simulate an initial shares message.
				msg.setIsInitialSharesMessage(true);
			} 

			if (msg.isInitialSharesMessage()) {
				logger.log(Level.INFO, "Received shares from peer: " + msg.getSenderID());
				IDRPeerInfo peerInfo = getPeerInfoByPeerID(msg.getSenderID());
				peerInfo.setNodeInfos(msg.getNodeInfos());
				// Add all nodes sent by this peer to nodeInfos
				for (IDRNodeInfo node : msg.getNodeInfos()) {
					long id = node.getID();
					if (id > numberOfNodes) {
						logger.warning("IDRPrivacyPeer got a NodeInfo with an out-of-bounds id; ignoring");
						continue;
					} else if (getNodeByID(id) != null) {
						// throw exception if we get duplicate node ID
						throw new Exception("IDRPrivacypeer received two nodes with the same ID: " + id);
					}
					if (fakeDaemon) {
						// for worst case analysis we set it so everyone always has some neighbor
						node.setFinalNextHop(getNeighborsByID(id)[0]);
					}
					nodeInfos.set((int) node.getID()-1, node);
				}

				initialSharesToReceive--;
				if (initialSharesToReceive <= 0) {
					logger.log(Level.INFO, "Received all initial shares from peers...");
					startNextPPProtocolStep();
				}

			} else {
				String errorMessage = "Didn't receive initial shares...";
				errorMessage += "\nisGoodBye: "+msg.isGoodbyeMessage();  
				errorMessage += "\nisHello: "+msg.isHelloMessage();
				errorMessage += "\nisInitialShares: "+msg.isInitialSharesMessage();
				errorMessage += "\nisFinalResult: "+msg.isFinalResultMessage();
				logger.log(Level.SEVERE, errorMessage);
				sendExceptionEvent(this, errorMessage);
			}
		} else {
			throw new Exception("Received unexpected message type (expected: " + IDRMessage.class.getName() + ", received: " + object.getClass().getName());
		}
	}


	/**
	 * returns the number of peers connected to this one
	 */
	public int getNumberOfInputPeers() {
		return numberOfInputPeers;
	}


	/**
	 * returns the number of time slots
	 */
	public int getTimeSlotCount() {
		return timeSlotCount;
	}


	/**
	 * @return the numberOfItems per time slot
	 */
	public int getNumberOfItems() {
		return numberOfItems;
	}

	public int getNumberOfNodes() {
		return numberOfNodes;
	}


	/**
	 * Returns the peer info for the PRIVACY PEER with the given user number, 
	 * which corresponds to the index of this privacy peer's elements in the list
	 * (null if user not in list)
	 *
	 * @param privacyPeerNumber	The privacy peer's number in the list
	 *
	 * @return The privacy peers info instance (null if not found)
	 */
	protected synchronized IDRPeerInfo getPrivacyPeerInfoByIndex(int privacyPeerNumber) {
		return privacyPeerInfos.elementAt(privacyPeerNumber);
	}


	/**
	 * Returns the peer info for the INPUT PEER with the given user number, which
	 * corresponds to the index of this privacy peer's elements in the list
	 * (null if user not in list)
	 *
	 * @param peerNumber	the peer's number in the list
	 *
	 * @return The input peers info instance (null if not found)
	 */
	protected synchronized IDRPeerInfo getPeerInfoByIndex(int peerNumber) {
		return peerInfos.elementAt(peerNumber);
	}

	/**
	 * Use this to get a node when you are traversing all the nodes in the list.
	 * @param nodeNumber
	 * @return
	 */
	protected synchronized IDRNodeInfo getNodeByIndex(int nodeNumber) {
		return nodeInfos.get(nodeNumber);
	}

	/**
	 * Use this to get a node when you know its ID number.
	 * @param nodeID
	 * @return
	 */
	protected synchronized IDRNodeInfo getNodeByID(long nodeID) {
		return nodeInfos.get((int)(nodeID - 1));
	}

	protected synchronized long[] getNeighborsByIndex(int nodeNumber) {
		return topology.get(nodeNumber);
	}

	protected synchronized long[] getNeighborsByID(long nodeID) {
		return topology.get((int)(nodeID - 1));
	}

	/**
	 * Returns the peer info for the PEER with the given peer ID.
	 * 
	 * @param peerID	The peer's ID
	 * 
	 * @return The peers info instance (null if not found)
	 */
	protected synchronized IDRPeerInfo getPeerInfoByPeerID(String peerID) {
		for (IDRPeerInfo peerInfo : peerInfos) {
			if (peerInfo.getID() == null) {
				logger.log(Level.WARNING, "There is a peerInfo without a peerID! " + peerInfo.getIndex());
			}
			else if (peerInfo.getID().equals(peerID)) {
				return peerInfo;
			}
		}
		return null;
	}

	/**
	 * 
	 * @return True if we have a random number generator setup, False else
	 */
	protected boolean randomReady() {
		return !(rand == null);
	}


	/**
	 * Wait until the privacy peer is ready for the next PeerProtocol step. 
	 * @throws InterruptedException
	 */
	public void waitForNextPeerProtocolStep() {
		logger.log(Level.INFO, "PeerProtocol Barrier: Thread nr. "+(peerProtocolBarrier.getNumberOfWaitingThreads()+1)+" arrived.");
		try {
			peerProtocolBarrier.block();
		} catch (InterruptedException e) {
			// ignore
		}
	}


	/**
	 * Starts the next PeerProtocol step. 
	 * @throws InterruptedException
	 */
	protected void startNextPeerProtocolStep() {
		logger.log(Level.INFO, "PeerProtocol Opening the barrier. PeerProtocol Threads can start the next step.");
		try {
			peerProtocolBarrier.openBarrier();
		} catch (InterruptedException e) {
			// ignore
		}
	}


	/**
	 * Wait until the privacy peer is ready for the next PPProtocol step. 
	 * @throws InterruptedException
	 */
	public void waitForNextPPProtocolStep() {
		logger.log(Level.INFO, "PPProtocol Barrier: Thread nr. "+(ppProtocolBarrier.getNumberOfWaitingThreads()+1)+" arrived.");
		try {
			ppProtocolBarrier.block();
		} catch (InterruptedException e) {
			// ignore
		}
	}


	/**
	 * Starts the next PPProtocol step. 
	 * @throws InterruptedException
	 */
	protected void startNextPPProtocolStep() throws InterruptedException {
		logger.log(Level.INFO, "PPProtocol Opening the barrier. PPProtocol Threads can start the next step.");
		ppProtocolBarrier.openBarrier();
	}


	/**
	 * starts the product computations
	 *
	public void startProductComputations() {
		logger.log(Level.INFO, Services.getFilterPassingLogPrefix()+ "STARTING IDR Protocol round...");
		int activeInputPeers = connectionManager.getNumberOfConnectedPeers(false, true);
		// create product operation set
		initializeNewOperationSet(numberOfItems);
		operationIDs = new int[numberOfItems];
		long[] data = null;
		for(int operationIndex = 0; operationIndex < numberOfItems; operationIndex++) {
			operationIDs[operationIndex] = operationIndex;
			data = new long[activeInputPeers];
			int dataIndex=0;
			for(int peerIndex = 0; peerIndex < numberOfInputPeers; peerIndex++) {
				long[] initialShares = getPeerInfoByIndex(peerIndex).getInitialShares();
				if(initialShares!=null) { // only consider active input peers
					data[dataIndex++] = initialShares[operationIndex];
				}
			}
			primitives.product(operationIndex, data);
		}
		logger.log(Level.INFO, "thread " + Thread.currentThread().getId() + " started the "+operationIDs.length+" product operations...");
	}
	 */

	/**
	 * Check to see if we should bother running the steps for our node this round.
	 * @param useISPs
	 * @return
	 */
	protected boolean goAhead(boolean useISPs) {
		IDRNodeInfo node = getNodeByID(nodeID);
		if (node == null)
			return false;
		if (node.isISP() != useISPs || node.isDestination())
			return false;
		return true;
	}

	/**
	 * This function (and the prepareRandoms) are left in in case
	 * we ever need the capability to generate random numbers.
	 * In our current version we should never actually be doing this.
	 * @return True if we need to generate a PRNG, false else.
	 */
	public boolean needRandom() {
		return false;
	}

	public void prepareRandom1() {
		initializeNewOperationSet(1);
		operationIDs = new int[1];
		operationIDs[0] = 0;
		primitives.generateRandomNumber(operationIDs[0], null);
	}

	public void prepareRandom2() {
		long[] result = {primitives.getResult(operationIDs[0])[0]};
		initializeNewOperationSet(1);
		operationIDs = new int[1];
		operationIDs[0] = 0;
		primitives.reconstruct(operationIDs[0], result);
	}

	public void prepareRandom3() {
		rand = new Random(primitives.getResult(0)[0]);
	}

	private long roundTimer, timer, totalTimer, firstTimer = 0;
	private int rounds = 0;

	/*
	 * Idea:
	 * 	For each neighbor Y:
	 * 		For each neighbor Z of neighbor Y:
	 * 			Compute whether Z is Y's next hop (Step 1)
	 * 			Multiply above by whether Y would send routes through Z to X (Step 2)
	 * 		Now we have a 0/1 share array for Y
	 * 		Sum this array to get A, A is 1 iff Y is sharing a route
	 * 		Force A to 1 if Y is the destination 
	 * 		Add Y's pathLength to X's classification for it to get B
	 * 		For each domain Z in Y's path:
	 * 			Check if Z=X (Step 3)
	 * 		Sum results of above to get C, C is 1 iff Y would be a loop
	 * 		Multiply (1-A+C) * 4M + B to get Y's score S
	 * 	Calculate the lowest S value s.t. S < 4M (Step 4)
	 *  Figure out which neighbors actually have this lowest S (Step 5)
	 *  Obtain the location of the first among those neighbors (Step 6)
	 *  Pick out the next hop / route and path length of that neighbor (Step 7)
	 *  Collect results (Step 8)
	 *  
	 */

	public void runStep1(boolean useISPs) {
		roundTimer = System.currentTimeMillis();
		timer = System.currentTimeMillis();

		IDRNodeInfo node = getNodeByID(nodeID);
		int opX = 0;
		for (long nbID : node.getNeighbors()) {
			IDRNodeInfo neighbor = getNodeByID(nbID);
			opX += neighbor.getNeighbors().length;
		}

		initializeNewOperationSet(opX);
		operationIDs = new int[opX];
		opX = 0;


		for (long nbID : node.getNeighbors()) {
			IDRNodeInfo neighbor = getNodeByID(nbID);
			for (long nbnbID : neighbor.getNeighbors()) {
				operationIDs[opX] = opX;
				primitives.equal(opX++, new long[]{neighbor.getNextHop(), nbnbID});
			}
		}
	}

	public void runStep2(boolean useISPs) {
		logger.log(Level.INFO, Services.getFilterPassingLogPrefix() + "Step 1 took " + (System.currentTimeMillis() - timer) + " ms");
		timer = System.currentTimeMillis();

		long[] equalShares = new long[operationIDs.length];

		for(int i=0; i < operationIDs.length; i++) {
			equalShares[i] = primitives.getResult(i)[0];
		}

		int opX = operationIDs.length;
		initializeNewOperationSet(opX);
		operationIDs = new int[opX];
		opX = 0;

		IDRNodeInfo node = getNodeByID(nodeID);

		for (long nbID : node.getNeighbors()) {
			IDRNodeInfo neighbor = getNodeByID(nbID);
			int i;
			for (i = 0; i < neighbor.getNeighbors().length; i++) {
				if (neighbor.getNeighbors()[i] == nodeID) {
					break;
				}
			}
			// now i is the location of this node in the neighbor's neighbor list
			for (int j = 0; j < neighbor.getNeighbors().length; j++) {
				operationIDs[opX] = opX;
				primitives.multiply(opX, new long[]{neighbor.getInitialExportShares()[i][j], equalShares[opX]}); 
				opX++;
			}
		}
	}

	long[] sharing;

	public void runStep3(boolean useISPs) {
		logger.log(Level.INFO, Services.getFilterPassingLogPrefix() + "Step 2 took " + (System.currentTimeMillis() - timer) + " ms");
		timer = System.currentTimeMillis();

		IDRNodeInfo node = getNodeByID(nodeID);
		sharing = new long[node.getNeighbors().length];
		int opX = 0;

		for (int i = 0; i < node.getNeighbors().length; i++) {
			IDRNodeInfo neighbor = getNodeByID(node.getNeighbors()[i]);
			for (int j = 0; j < neighbor.getNeighbors().length; j++) {
				sharing[i] += primitives.getResult(opX++)[0];
			}
			// If i is the destination and it's our neighbor, then we just assume it has a route
			if (neighbor.isDestination()) {
				sharing[i] = 1;
			}
		}
		// At this point, sharing[i] is a share of 1 iff neighbor i is sharing a route.

		// Now we are going to check neighbor i's path to see if it has this node's id

		opX = 0;

		for (int i = 0; i < node.getNeighbors().length; i++) {
			IDRNodeInfo neighbor = getNodeByID(node.getNeighbors()[i]);
			for (int j = 0; j < neighbor.getRoute().length; j++) {
				opX++;
			}
		}


		initializeNewOperationSet(opX);
		operationIDs = new int[opX];
		opX = 0;

		for (int i = 0; i < node.getNeighbors().length; i++) {
			IDRNodeInfo neighbor = getNodeByID(node.getNeighbors()[i]);
			for (int j = 0; j < neighbor.getRoute().length; j++) {
				operationIDs[opX] = opX;
				primitives.equal(opX++, new long[]{neighbor.getRoute()[j], nodeID});
			}
		}
	}	

	long[] score;

	public void runStep4(boolean useISPs) {
		logger.log(Level.INFO, Services.getFilterPassingLogPrefix() + "Step 3 took " + (System.currentTimeMillis() - timer) + " ms");
		timer = System.currentTimeMillis();

		IDRNodeInfo node = getNodeByID(nodeID);
		long[] loop = new long[node.getNeighbors().length];

		int opX = 0;

		for (int i = 0; i < node.getNeighbors().length; i++) {
			IDRNodeInfo neighbor = getNodeByID(node.getNeighbors()[i]);
			for (int j = 0; j < neighbor.getRoute().length; j++) {
				loop[i] += primitives.getResult(opX++)[0];
			}
		}
		//now loop[i] == 1 iff route through i would cause a loop

		// S = (1-sharing[i]+loop[i]) *4M + B
		score = new long[node.getNeighbors().length];
		for (int i = 0; i < node.getNeighbors().length; i++) {
			IDRNodeInfo neighbor = getNodeByID(node.getNeighbors()[i]);
			long A = 0;
			long B = node.getInitialClassificationShares()[i] + neighbor.getPathLength();
			long C = 0;
			// Want to do (1-sharing[i])*4M
			// Since 4M is a constant, it is easier to add locally than multiply using SEPIA
			for (int z = 0; z < 4*M; z++) {
				A += 1 - sharing[i];
				C += loop[i];
			}
			score[i] = A + B + C;
		}

		initializeNewOperationSet(1);
		operationIDs = new int[]{0};
		primitives.min(0, score, -1, true); // the -1 and true might need to be changed for optimization
	}

	public void runStep5(boolean useISPs) {
		logger.log(Level.INFO, Services.getFilterPassingLogPrefix() + "Step 4 took " + (System.currentTimeMillis() - timer) + " ms");
		timer = System.currentTimeMillis();
		long min = primitives.getResult(0)[0];
		// min is the score of the nextHop we are taking; now we want to know which neighbor it corresponds to

		IDRNodeInfo node = getNodeByID(nodeID);
		initializeNewOperationSet(node.getNeighbors().length);
		operationIDs = new int[node.getNeighbors().length];
		for (int opX = 0; opX < node.getNeighbors().length; opX++) {
			operationIDs[opX] = opX;
			primitives.equal(opX, new long[]{min, score[opX]});
		}
	}

	public void runStep6(boolean useISPs) {
		logger.log(Level.INFO, Services.getFilterPassingLogPrefix() + "Step 5 took " + (System.currentTimeMillis() - timer) + " ms");
		timer = System.currentTimeMillis();

		long [] winner = new long[operationIDs.length];
		for (int i = 0; i < winner.length; i++) {
			winner[i] = primitives.getResult(i)[0];
		}

		/*
		 * Now winner is a list of shares of 0/1, 1 if this is the neighbor with the minimum score.
		 * The problem is that there might be more than one 1 in this list.
		 * So we have to eliminate duplicates.
		 * In closing, the ID of the node we want to use as nextHop is the sum of the following
		 * over all ids:
		 * id[i] * winner[i] * sum[ (1-winner[0]), ..., (1-winner[i-1]) 
		 */

		IDRNodeInfo node = getNodeByID(nodeID);
		initializeNewOperationSet(node.getNeighbors().length);
		operationIDs = new int[node.getNeighbors().length];
		int opX = 0;

		for (int i = 0; i < node.getNeighbors().length; i++) {
			long[] data;
			if (i == 0)
				data = new long[2];
			else
				data = new long[1+i];
			data[0] = winner[i];
			if (i == 0)
				data[1] = 1; // dummy input
			for (int w = 0; w < i; w++) {
				data[1 + w] = 1-winner[w];
			}
			operationIDs[opX] = opX;
			primitives.product(operationIDs[opX], data);
			opX++;
		}
	}

	public void runStep7(boolean useISPs) {
		logger.log(Level.INFO, Services.getFilterPassingLogPrefix() + "Step 6 took " + (System.currentTimeMillis() - timer) + " ms");
		timer = System.currentTimeMillis();

		long [] winner = new long[operationIDs.length];
		for (int i = 0; i < winner.length; i++) {
			winner[i] = primitives.getResult(i)[0];
		}
		// now winner is 1 ONLY for the first result with the min score
		// so now all we need to do is distribute this time all the routes and pathLengths



		IDRNodeInfo node = getNodeByID(nodeID);

		int opX = 0;
		for (int i = 0; i < node.getNeighbors().length; i++) {
			opX ++;
			opX += getNodeByID(node.getNeighbors()[i]).getRoute().length;
		}

		initializeNewOperationSet(opX);
		operationIDs = new int[opX];
		opX = 0;

		for (int i = 0; i < node.getNeighbors().length; i++) {
			IDRNodeInfo neighbor = getNodeByID(node.getNeighbors()[i]);
			operationIDs[opX] = opX;
			primitives.product(opX++, new long[]{neighbor.getPathLength(), winner[i]});
			for (int j = 0; j < neighbor.getRoute().length; j++) {
				operationIDs[opX] = opX;
				primitives.product(opX++, new long[]{neighbor.getRoute()[j], winner[i]});
			}
		}	
	}

	public void runStep8(boolean useISPs) {
		logger.log(Level.INFO, Services.getFilterPassingLogPrefix() + "Step 7 took " + (System.currentTimeMillis() - timer) + " ms");
		timer = System.currentTimeMillis();

		IDRNodeInfo node = getNodeByID(nodeID);

		pathLength = 0;
		route = new long[M]; // because for any neighbor, neighbor.getRoute().length == M
		int opX = 0;
		for (int i = 0; i < node.getNeighbors().length; i++) {
			pathLength += primitives.getResult(opX++)[0];
			for (int j = 0; j < M; j++)
				route[j] += primitives.getResult(opX++)[0];
		}

		// now pathLength and route are correctly shares of the new pathLength and route!
		// note that these need to be updated to reflect the fact that we are the first thing on the path
		// this happens later in node.addToRoute


	}

	protected void processPrimitives() {
		try {
			do {
				primitives.processReceivedData();
			} while (!primitives.areOperationsCompleted());
		} catch (Exception e) {
			logger.severe("Something bad happened in processPrimitives.");
			return;
		}
	}

	public void setAllNextHops(boolean useISPs) {
		if (goAhead(useISPs)) {
			logger.log(Level.INFO, Services.getFilterPassingLogPrefix() + "Step 8 took " + (System.currentTimeMillis() - timer) + " ms");
			long roundTime = System.currentTimeMillis() - roundTimer;
			if (rounds == 0)
				firstTimer = roundTime;
			totalTimer += roundTime;
			rounds ++;
			logger.log(Level.INFO, Services.getFilterPassingLogPrefix() + "This round took " + roundTime + " ms");
		}

		// Important: If we're working for the destination node,
		// path length and route never change
		if (getNodeByID(nodeID).isDestination()) {
			//System.out.println("Destination; sending " + nodeID);
			pathLength = getNodeByID(nodeID).getPathLength();
			route = getNodeByID(nodeID).getRoute();
		} else {
			//System.out.println("Sending " + nextHop);
		}

		if (fakeDaemon) {
			for (int i = 0; i < numberOfNodes; i++) {				
				IDRNodeInfo node = getNodeByIndex(i);
				if (node == null)
					continue;
				if (node.getID() == nodeID){
					node.setPathLength(pathLength);
					node.setRoute(route);
					node.addToRoute(nodeID);
					continue;
				}
				if (node.isDestination()) {
					continue;
				}

				// make up something
				node.setPathLength(2);
				long[] newRoute = new long[M];
				newRoute[0] = getNeighborsByIndex(i)[0];
				node.setRoute(newRoute);
				node.addToRoute(node.getID());


			}
		} else { // !fakeDaemon
			try {
				// Send this node's pathLength and nextHop to the Daemon
				long[] toSend = new long[1+M];
				toSend[0] = pathLength;
				for (int i = 0; i < route.length; i++) {
					toSend[i+1] = route[i];
				}
				outStream.writeObject(toSend);
				outStream.flush();

				// Wait for the Daemon to tell us a list of all pathLengths and nextHops
				long[][] results = (long[][])inStream.readObject();


				// Note: The ids of the nodes should be 1...V
				// The results array should go from 0...V but the 0 entry is blank
				if (results.length > numberOfNodes + 1) {
					throw new IOException("Array of results from daemon is too large");
				}

				if (results.length < numberOfNodes + 1) {
					throw new IOException("Array of results from daemon is too small");
				}

				for (int i = 1; i <= numberOfNodes; i++) {
					IDRNodeInfo node = getNodeByID(i);
					if (node == null)
						continue;
					node.setPathLength(results[i][0]);
					long[] newRoute = new long[M];
					for (int j = 0; j < newRoute.length; j++) {
						newRoute[j] = results[i][j+1];
					}
					node.setRoute(newRoute);
					node.addToRoute(node.getID());
				}
			} catch (IOException e) {
				System.err.println("Error communicating with the daemon");
				e.printStackTrace();
			} catch (ClassNotFoundException e) {
				e.printStackTrace();
			}
		}
	}

	/**
	 * Reconstructs X.
	 * @param X
	 * @return
	 */
	public long reconstruct(long X){
		initializeNewOperationSet(1);
		long [] data = {X};
		if(!primitives.reconstruct(0, data)) {
			logger.log(Level.SEVERE, "reconstruct operation arguments are invalid: id="+0  +", data="+data[0]);
		}
		return primitives.getResult(0)[0];
	}


	/**
	 * starts the reconstruction of the final result
	 */
	public void scheduleFinalResultReconstruction() {
		initializeNewOperationSet(2 * numberOfNodes);
		operationIDs = new int[2 * numberOfNodes];
		int opX = 0;
		for(int i = 0; i < numberOfNodes; i++) {
			// create reconstruction operation for result of product operation
			operationIDs[opX] = opX;
			primitives.reconstruct(opX++, new long[]{getNodeByIndex(i).getPathLength()});
			operationIDs[opX] = opX;
			primitives.reconstruct(opX++, new long[]{getNodeByIndex(i).getFinalNextHop()});
		}
	}

	public void finishFinalResultReconstruction() {
		int opX = 0;
		for (int i = 0; i < numberOfNodes; i++) {
			IDRNodeInfo node = getNodeByIndex(i);
			node.setPathLength(primitives.getResult(opX++)[0]);
			node.setFinalNextHop(primitives.getResult(opX++)[0]);
		}
	}

	public void reportTimings() {
		logger.log(Level.INFO, Services.getFilterPassingLogPrefix()+"Total time for " + rounds + " round" + (rounds == 1 ? ": " : "s: ") + totalTimer + " ms");
		logger.log(Level.INFO, Services.getFilterPassingLogPrefix()+"Average round time (all rounds): "
				+ ((double)totalTimer / rounds) + " ms");
		logger.log(Level.INFO, Services.getFilterPassingLogPrefix()+"Average round time (excluding first): "
				+ ((double)(totalTimer - firstTimer) / (rounds-1)) + " ms");

		logger.log(Level.INFO, "thread " + Thread.currentThread().getId() + " started the final result reconstruction; (" + operationIDs.length + " reconstruction operations are in progress)");
	}


	/**
	 * retrieves and stores the final result
	 */
	public void setFinalResult() {
		logger.info("Thread " + Thread.currentThread().getId() + " starts next pp-peer protocol step...");
		startNextPeerProtocolStep();
	}

	/**
	 * Returns the final result to report to the peer with ID peerID.
	 * @param peerID
	 * @return
	 */
	public long[][] getFinalResult(int peerID) {
		IDRPeerInfo peer = getPeerInfoByIndex(peerID);
		long[][] results = new long[peer.getNodeInfos().length][2];
		int n = 0;
		for (int i = 0; i < numberOfNodes && n < results.length; i++) {
			IDRNodeInfo node = peer.getNodeInfos()[i];
			if (node == null)
				continue;
			if (!node.getPeerID().equals(peer.getID()))
				continue;
			results[n][0] = node.getPathLength();
			results[n][1] = node.getFinalNextHop();
			n++;
		}
		return results;
	}

	/**
	 * lets protocol thread report to privacy peer that it sent the final result and
	 * starts new round if there are more time slots (data) to process
	 */
	protected synchronized void finalResultIsSent() {
		finalResultsToDo--;
		logger.log(Level.INFO, "thread " + Thread.currentThread().getId() + " called finalResultIsSent; finalResultsToDo="+finalResultsToDo);
		if(finalResultsToDo <= 0) {
			// report final result to observers
			logger.log(Level.INFO, Services.getFilterPassingLogPrefix()+ "Sent all final results. Notifying observers...");
			VectorData dummy = new VectorData(); // dummy data to avoid null pointer exception in Peers::processMpcEvent
			FinalResultEvent finalResultEvent;
			finalResultEvent = new FinalResultEvent(this, myAlphaIndex, getMyPeerID(), getMyPeerID(), dummy);
			finalResultEvent.setVerificationSuccessful(true);
			sendNotification(finalResultEvent);
			// check if there are more time slots to process
			if(currentTimeSlot < timeSlotCount) {
				currentTimeSlot++;
				logger.log(Level.INFO, "thread " + Thread.currentThread().getId() + " increased currentTimeSlot to "+currentTimeSlot+", will init new round now...");
				initializeNewRound();
			} else {
				logger.log(Level.INFO, "No more data available... Stopping protocol threads...");
				protocolStopper.setIsStopped(true);
			}
		}
	}
}
