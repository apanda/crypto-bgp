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
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Observable;
import java.util.Properties;
import java.util.Vector;
import java.util.concurrent.CyclicBarrier;
import java.util.logging.Level;
import java.util.logging.Logger;

import mpc.ShamirSharing;
import mpc.VectorData;
import mpc.protocolPrimitives.PrimitivesEnabledProtocol;
import services.Services;
import services.Stopper;
import startup.ConfigFile;
import connections.ConnectionManager;
import events.FinalResultEvent;

/**
 * A MPC peer providing the private input data for the topk protocol
 *
 * @author Dilip Many
 *
 */
public class IDRPeer extends IDRBase {

	/** vector of protocols (between this peer and the privacy peers) */
	private Vector<IDRProtocolPeer> peerProtocolThreads = null;
	/** MpcShamirSharing instance to use basic operations on Shamir shares */
	protected ShamirSharing mpcShamirSharing = null;

	/** barrier to synchronize the protocol threads of this peer*/
	private CyclicBarrier protocolThreadsBarrier = null;

	/** array containing my initial shares; dimensions [numberOfNodes][numberOfPrivacyPeers][numberOfNeighbors] */
	private long[][][] initialPrefShares = null;
	/** array containing initial shares; dimensions [numberOfNodes][numberOfNeighbors][numberOfPrivacyPeers][numberOfNeighbors] */
	private long[][][][] initialExportShares = null;
	/** array containing initial shares of the matches arrays; dimensions [numberOfNodes][numberOfNeighbors][numberOfNeighbors][numberOfPrivacyPeers] */
	private long[][][][] initialMatchesShares = null;

	/**
	 * Matches array: matches[i][j] = 1 iff neighbor[i] = prefList[j]
	 */
	
	public static final String PROP_IDR_INPUT = "mpc.idr.input";
	
	public static final String PROP_DESTINATION = "Destination";
	public static final String PROP_N_ITEMS = "nitems";
	public static final String PROP_PREFERENCES = "Preferences";
	public static final String ID = "peerID";
	public static final String PROP_TYPE = "peerType";

	protected int numberOfNodes;

	protected IDRNodeInfo[] nodeInfos = null;

	/** filename of the input file*/
	private String input_filename;

	/** ID of the destination */
	protected long destination;

	/** ID number-order list of neighbors; dimensions[numberOfNeighbors] */
	//protected long[] neighborList;

	/** Preference-order list of neighbors for each node; dimensions [numberOfNodes][numberOfNeighbors] */
	protected long[][] neighborPreferenceLists;

	/** Array of lists of who can be routed through each neighbor; dimensions [numberOfNodes][numberOfNeighbors][numberOfNeighbors]
	 * Example: neighborExports[0][0] is the list of which neighbors can receive routes through
	 *     neighborPreferenceList[0] for node 0
	 */
	protected long[][][] neighborExports;
	
	private Properties properties;
	private boolean readOperationSuccessful;

	/**
	 * constructs a new topk peer object
	 *
	 * @param myPeerIndex	This peer's number/index
	 * @param stopper		Stopper (can be used to stop this thread)
	 * @param cm the connection manager
	 * @throws Exception
	 */
	public IDRPeer(int myPeerIndex, ConnectionManager cm, Stopper stopper) throws Exception {
		super(myPeerIndex, cm, stopper);
		peerProtocolThreads = new Vector<IDRProtocolPeer>();
		mpcShamirSharing = new ShamirSharing();
	}

	/**
	 * Initializes the peer
	 */
	public void initialize() throws Exception {
		initProperties();
		properties = new Properties();

		mpcShamirSharing.setRandomAlgorithm(randomAlgorithm);
		mpcShamirSharing.setFieldSize(shamirSharesFieldOrder);
		if (degreeT>0) {
			mpcShamirSharing.setDegreeT(degreeT);
		}

		currentTimeSlot = 1;

	}

	/**
	 * Initializes and starts a new round of computation. It first (re-)established connections and
	 * then creates and runs the protocol threads for the new round. 
	 */
	protected void initializeNewRound(){
		connectionManager.waitForConnections();
		connectionManager.activateTemporaryConnections();
		PrimitivesEnabledProtocol.newStatisticsRound();

		List<String> privacyPeerIDs = connectionManager.getActivePeers(true);
		Collections.sort(privacyPeerIDs);
		numberOfPrivacyPeers = privacyPeerIDs.size();
		mpcShamirSharing.setNumberOfPrivacyPeers(numberOfPrivacyPeers);
		mpcShamirSharing.init();
		clearPP2PPBarrier();

		protocolThreadsBarrier = new CyclicBarrier(numberOfPrivacyPeers);

		// Init state variables
		finalResult = null;
		finalResultsToDo = numberOfPrivacyPeers;
		neighborPreferenceLists = null;
		neighborExports = null;
		nodeInfos = null;

		readPrivateData(input_filename);
		readTopology(topo_filename);
		
		if (!readOperationSuccessful) {
			logger.severe("Could not read private input from " + topo_filename + " -- returning");
			return;
		}
		createProtocolThreadsForPrivacyPeers(privacyPeerIDs);
	}


	/**
	 * Create and start the threads. Attach one privacy peer id to each of them.
	 * 
	 * @param privacyPeerIDs the ids of the privacy peers
	 */
	private void createProtocolThreadsForPrivacyPeers(List<String> privacyPeerIDs)  { 
		peerProtocolThreads.clear();
		int currentID = 0;
		for(String ppId: privacyPeerIDs) {
			logger.log(Level.INFO, "Create a thread for privacy peer " +ppId );
			IDRProtocolPeer iDRProtocolPeer = new IDRProtocolPeer(currentID, this, ppId, currentID, stopper);
			iDRProtocolPeer.addObserver(this);
			Thread thread = new Thread(iDRProtocolPeer, "Topk Peer protocol with user number " + currentID);
			peerProtocolThreads.add(iDRProtocolPeer);
			thread.start();
			currentID++;
		}
	}


	/**
	 * Generates shares for each secret input.
	 * Also generates shares of matches[][].
	 */
	public void generateInitialShares() {
		logger.log(Level.INFO, "Generating initial shares...");
		
		initialPrefShares = new long[numberOfNodes][numberOfPrivacyPeers][];
		for (int i = 0; i < numberOfNodes; i++) {
			initialPrefShares[i] = mpcShamirSharing.generateShares(neighborPreferenceLists[i]);
		}

		initialExportShares = new long[numberOfNodes][][][];

		for(int i = 0; i < numberOfNodes; i++) {
			initialExportShares[i] = new long[neighborExports[i].length][][];
			for (int j = 0; j < initialExportShares[i].length; j++) {
				initialExportShares[i][j] = mpcShamirSharing.generateShares(neighborExports[i][j]);
			}
		}
		
		initialMatchesShares = new long[numberOfNodes][][][];
		
		for (int n = 0; n < numberOfNodes; n++) {
			long[] neighbors = nodeInfos[n].getNeighbors();
			initialMatchesShares[n] = new long[neighbors.length][neighborPreferenceLists[n].length][];
			for (int i = 0; i < neighbors.length; i++) {
				for (int j = 0; j < neighborPreferenceLists[n].length; j++) {
					initialMatchesShares[n][i][j] = mpcShamirSharing.generateShare(
							neighbors[i] == neighborPreferenceLists[n][j] ? 1 : 0);
				}
			}
		}
	}


	/**
	 * Run the MPC protocol(s) over the given connection(s).
	 */
	public void runProtocol() throws Exception {
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
			// We are awaiting a final results message				
			IDRMessage message = (IDRMessage) object;

			if(message.isDummyMessage()) {
				// Simulate a final results message in order not to stop protocol execution
				message.setIsFinalResultMessage(true);
			}

			if(message.isFinalResultMessage()) {
				logger.log(Level.INFO, "Received a final result message from a privacy peer");
				finalResultsToDo--;

				if (finalResult == null && message.getFinalResults() != null) {
					finalResult = message.getFinalResults();
				}

				if(finalResultsToDo <= 0) {
					// notify observers about final result
					logger.log(Level.INFO, Services.getFilterPassingLogPrefix()+ "Received all final results. Notifying observers...");
					VectorData dummy = new VectorData(); // dummy data to avoid null pointer exception in Peers::processMpcEvent
					FinalResultEvent finalResultEvent = new FinalResultEvent(this, myAlphaIndex, getMyPeerID(), message.getSenderID(), dummy);
					finalResultEvent.setVerificationSuccessful(true);
					sendNotification(finalResultEvent);

					writeOutputToFile();

					System.out.println("NextHop: " + finalResult.toString()); // This is for debugging only

					// check if there are more time slots to process
					if(currentTimeSlot < timeSlotCount) {
						currentTimeSlot++;
						initializeNewRound();
					} else {
						logger.log(Level.INFO, "No more data available... Stopping protocol threads...");
						protocolStopper.setIsStopped(true);
					}
				}
			} else {
				String errorMessage = "Didn't receive final result...";
				errorMessage += "\nisGoodBye: "+message.isGoodbyeMessage();  
				errorMessage += "\nisHello: "+message.isHelloMessage();
				errorMessage += "\nisInitialShares: "+message.isInitialSharesMessage();
				errorMessage += "\nisFinalResult: "+message.isFinalResultMessage();
				logger.log(Level.SEVERE, errorMessage);
				sendExceptionEvent(this, errorMessage);
			}
		} else {
			throw new Exception("Received unexpected message type (expected: " + IDRMessage.class.getName() + ", received: " + object.getClass().getName());
		}
	}

	/**
	 * Read the private data from the file.
	 * @param filename
	 * @return
	 */
	protected boolean readPrivateData(String filename) {
		readOperationSuccessful = false;
		try {
			FileInputStream in = new FileInputStream(filename);
			properties.load(in);
			in.close();
			numberOfNodes = Integer.parseInt(properties.getProperty(PROP_N_ITEMS));
			nodeInfos = new IDRNodeInfo[numberOfNodes];
			destination = Long.parseLong(properties.getProperty(PROP_DESTINATION));
			neighborPreferenceLists = new long[numberOfNodes][];
			neighborExports = new long[numberOfNodes][][];
			for (int i = 0; i < nodeInfos.length; i++) {
				String prefix = (i+1) + "_";
				nodeInfos[i] = new IDRNodeInfo(Long.parseLong(properties.getProperty(prefix + ID)), myPeerID);
				nodeInfos[i].setISP(properties.getProperty(prefix + PROP_TYPE).equals("1"));
				if (nodeInfos[i].getID() == destination)
					nodeInfos[i].setDestination(true);
				String[] prefStrings = properties.getProperty(prefix + PROP_PREFERENCES).split("\\D");
				neighborPreferenceLists[i] = new long[prefStrings.length];
				for (int j = 0; j < neighborPreferenceLists[i].length; j++)
					neighborPreferenceLists[i][j] = Long.parseLong(prefStrings[j]);
				long[] neighborList = neighborPreferenceLists[i].clone();
				Arrays.sort(neighborList);
				neighborExports[i] = new long[neighborList.length][neighborList.length];
				if (nodeInfos[i].isISP()) {
					for (int j = 0; j < neighborList.length; j++) {
						String[] exportStrings = properties.getProperty(prefix + neighborList[j],"").split("\\D");
						for (int k = 0; k < exportStrings.length; k++) {
							try {
								neighborExports[i][j][k] = Long.parseLong(exportStrings[k]);
							} catch (NumberFormatException nfe) {
								neighborExports[i][j][k] = -1L;
							}
						}
						for (int k = exportStrings.length; k < neighborExports[i][j].length; k++) {
							neighborExports[i][j][k] = -1L;
						}
					}
				}
			}
			readOperationSuccessful = true;
		} catch (IOException e) {
			e.printStackTrace();
		}
		return readOperationSuccessful;
	}

	protected boolean readTopology(String filename) {
		try {
			FileInputStream in = new FileInputStream(filename);
			properties.load(in);
			in.close();
			for (IDRNodeInfo node : nodeInfos) {
				String nodeName = node.getID() + "";
				String[] neighborStrings = properties.getProperty(nodeName,"").split("\\D");
				long[] neighbors = new long[neighborStrings.length];
				for (int j = 0; j < neighborStrings.length; j++) {
					try {
						neighbors[j] = Long.parseLong(neighborStrings[j]);
					} catch (NumberFormatException nfe) {
						neighbors[j] = -1L;
					}
				}
				node.setNeighbors(neighbors);
			}
			readOperationSuccessful = readOperationSuccessful && true;
		} catch (IOException e) {
			e.printStackTrace();
			readOperationSuccessful = false;
		}
		return readOperationSuccessful;
	}
	
	/**
	 * Write the output to a file.
	 * @throws Exception 
	 */
	protected void writeOutputToFile() throws Exception {
		String fileName = outputFolder + "/" + "idr_output_"
				+ "_round" + String.format("%03d", currentTimeSlot)+ ".txt";
		String output = "";
		for (int i = 0; i < numberOfNodes; i++) {
			output += "Next Hop for " + nodeInfos[i].getID() + ": " + finalResult[i] + "\n";
		}
		Services.writeFile(output, fileName);
	}

	/**
	 * Infers from input file name whether it is a distribution of IP addresses or not.
	 * @return true if keys are IP addresses.
	 * /
	public boolean areKeysIpAddresses() {
		return topkData.isIPv4Distribution();

	}*/ // uncomment to restore method

	@Override
	protected synchronized void initProperties() throws Exception {
		super.initProperties();

		Properties properties = ConfigFile.getInstance().getProperties();
		// Set properties specific to IDR input peers
		input_filename = properties.getProperty(PROP_IDR_INPUT);
		topo_filename = properties.getProperty(PROP_IDR_TOPO_FILE);
		logger.log(Level.INFO, "IDR neighbor info filename="+topo_filename);
	}

	public CyclicBarrier getProtocolThreadsBarrier() {
		return protocolThreadsBarrier;
	}

	/**
	 * Returns the preference shares for each node.
	 * @param ppNr the PP number
	 * @return the shares. Dimensions: [numberOfNodes][numberOfNeighbors]
	 */
	public long[][] getInitialPrefSharesForPP(int ppNr) {
		long [][] shares = new long[numberOfNodes][];
		for (int i = 0; i < numberOfNodes; i++) {
			shares[i] = initialPrefShares[i][ppNr];
		}
		return shares;		
	}

	/**
	 * Returns the export shares for each node.
	 * @param ppNr the PP number
	 * @return the shares. Dimensions: [numberOfNodes][numberOfNeighbors][numberOfNeighbors]
	 */
	public long[][][] getInitialExportSharesForPP(int ppNr) {
		long[][][] shares = new long[numberOfNodes][][];
		for(int i = 0; i< numberOfNodes; i++) {
			shares[i] = new long[initialExportShares[i].length][];
			for (int j = 0; j < shares[i].length; j++) {
				shares[i][j] = initialExportShares[i][j][ppNr];
			}
		}
		return shares;		
	}
	
	/**
	 * Returns the matches array shares for each node.
	 * @param ppNr the PP number
	 * @return the shares. Dimensions: [numberOfNodes][numberOfNeighbors][numberOfNeighbors]
	 */
	public long[][][] getInitialMatchesSharesForPP(int ppNr) {
		long[][][] shares = new long[initialMatchesShares.length][][];
		for (int i = 0; i < initialMatchesShares.length; i++) {
			shares[i] = new long[initialMatchesShares[i].length][];
			for (int j = 0; j < initialMatchesShares[i].length; j++) {
				shares[i][j] = new long[initialMatchesShares[i][j].length];
				for (int k = 0; k < initialMatchesShares[i][j].length; k++) {
					shares[i][j][k] = initialMatchesShares[i][j][k][ppNr];
				}
			}
		}
		return shares;
	}

}
