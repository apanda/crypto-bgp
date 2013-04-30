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
	private long[][][] initialClassificationShares = null;
	/** array containing initial shares of forbidden list; dimensions [numberOfNodes][numberOfPrivacyPeers][numberOfForbidden] */
	private long[][][] initialForbiddenShares = null;
	/** array containing initial shares; dimensions [numberOfNodes][numberOfNeighbors][numberOfPrivacyPeers][numberOfNeighbors] */
	private long[][][][] initialExportShares = null;
	/** array containing M+1 zero shares/node; dimensions [numberOfNodes][numberOfPrivacyPeers][M+1] */
	private long[][][] initialZeroShares = null;

	public static final String PROP_IDR_INPUT = "mpc.idr.input";

	public static final String PROP_DESTINATION = "Destination";
	public static final String PROP_N_ITEMS = "nitems";
	public static final String PROP_PROVIDERS = "providers";
	public static final String PROP_PEERS = "peers";
	public static final String PROP_CUSTOMERS = "customers";
	public static final String PROP_FORBIDDEN = "forbidden";
	public static final String ID = "peerID";
	public static final String PROP_TYPE = "peerType";

	protected int numberOfNodes;

	protected IDRNodeInfo[] nodeInfos = null;

	/** filename of the input file*/
	private String input_filename;

	/** ID of the destination */
	protected long destination;
	
	/** Lists of customers, peers, providers for each node;
	 *  dimensions [numberOfNodes][numberOfLevels][numberOfNeighbors]
	 */
	protected long[][][] neighborClassificationLists;
	
	/** Lists of customers, peers, providers for each node;
	 *  dimensions [numberOfNodes][numberOfForbiddenNodes]
	 */
	protected long[][] neighborForbiddenLists;

	/** Array of lists of who can be routed through each neighbor;
	 *  dimensions [numberOfNodes][numberOfNeighbors][numberOfNeighbors]
	 *  Example: neighborExports[1][2] is the list of which neighbors can receive routes through
	 *  neighbor[2] for node 1
	 */
	protected long[][][] neighborExports;

	private Properties properties;
	private boolean readOperationSuccessful;

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
		neighborClassificationLists = null;
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

		initialClassificationShares = new long[numberOfNodes][numberOfPrivacyPeers][];
		// What's going on here:
		// For each neighbor of i, we search for it in the customer/peer/provider list for i.
		// If we find it, we save the number of the list we found it in to i's classList 
		// multiplied by M 
		for (int i = 0; i < numberOfNodes; i++) {
			long[] classList = new long[nodeInfos[i].getNeighbors().length];
			for (int h = 0; h < nodeInfos[i].getNeighbors().length; h++) {
				classList[h] = Integer.MAX_VALUE/2;
				for (int j = 0; j < numberOfLevels; j++) {
					for (int k = 0; k < neighborClassificationLists[i][j].length; k++) {
						if (neighborClassificationLists[i][j][k] == nodeInfos[i].getNeighbors()[h]) {
							classList[h] = j * M;
							j = numberOfLevels; // break out of two loops
							break;
						}
					}
				}
			}
			initialClassificationShares[i] = mpcShamirSharing.generateShares(classList);
		}
		
		initialForbiddenShares = new long[numberOfNodes][][];
		for (int i = 0; i < numberOfNodes; i++) {
			initialForbiddenShares[i] = mpcShamirSharing.generateShares(neighborForbiddenLists[i]);
		}
		

		initialExportShares = new long[numberOfNodes][][][];

		for(int i = 0; i < numberOfNodes; i++) {
			initialExportShares[i] = new long[neighborExports[i].length][][];
			for (int j = 0; j < initialExportShares[i].length; j++) {
				initialExportShares[i][j] = mpcShamirSharing.generateShares(neighborExports[i][j]);
			}
		}

		initialZeroShares = new long[numberOfNodes][numberOfPrivacyPeers][M+1];
		for(int i = 0; i < numberOfNodes; i++) {
			initialZeroShares[i] = mpcShamirSharing.generateShares(new long[M+1]);
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

					// Note that this prints to both STDOUT and the file
					System.out.println(writeOutputToFile());

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

	private static long[] concatAll(long[][] allArrays) {
		int totalLength = 0;
		for (long[] array : allArrays) {
			if (array == null)
				continue;
			totalLength += array.length;
		}
		long[] result = Arrays.copyOf(allArrays[0], totalLength);
		int offset = allArrays[0].length;
		for (int i = 1; i < allArrays.length; i++) {
			long[] array = allArrays[i];
			if (allArrays[i] == null)
				continue;
			System.arraycopy(array, 0, result, offset, array.length);
			offset += array.length;
		}
		return result;
	}

	private static long[] noZeroes(long[] array) {
		if (array == null)
			return null;
		int z = 0;
		for (long x : array) {
			if (x == 0) {
				z++;
			}
		}
		long[] newArray = new long[array.length - z];
		int o = 0, n = 0;
		while (n < newArray.length) {
			if (array[o] == 0) {
				o++;
			} else {
				newArray[n] = array[o];
				n++;
				o++;
			}
		}
		return newArray;		
	}
	/* UNCOMMENT THIS IF NEEDED
	private static long[] concatAll(long[] first, long[]... rest) {
		int totalLength = first.length;
		for (long[] array : rest) {
			totalLength += array.length;
		}
		long[] result = Arrays.copyOf(first, totalLength);
		int offset = first.length;
		for (long[] array : rest) {
			System.arraycopy(array, 0, result, offset, array.length);
			offset += array.length;
		}
		return result;
		//lol
	}



	private static <T> T[] concatAll(T[] first, T[]... rest) {
		int totalLength = first.length;
		for (T[] array : rest) {
			totalLength += array.length;
		}
		T[] result = Arrays.copyOf(first, totalLength);
		int offset = first.length;
		for (T[] array : rest) {
			System.arraycopy(array, 0, result, offset, array.length);
			offset += array.length;
		}
		return result;
	}
	 */

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
			neighborClassificationLists = new long[numberOfNodes][numberOfLevels][];
			neighborForbiddenLists = new long[numberOfNodes][];
			neighborExports = new long[numberOfNodes][][];
			for (int i = 0; i < nodeInfos.length; i++) {
				String prefix = (i+1) + "_";
				nodeInfos[i] = new IDRNodeInfo(Long.parseLong(properties.getProperty(prefix + ID)), myPeerID);
				nodeInfos[i].setISP(!properties.getProperty(prefix + PROP_TYPE).equals("0"));
				if (nodeInfos[i].getID() == destination)
					nodeInfos[i].setDestination(true);

				String customers = properties.getProperty(prefix + PROP_CUSTOMERS);
				String peers = properties.getProperty(prefix + PROP_PEERS);
				String providers = properties.getProperty(prefix + PROP_PROVIDERS);


				String classifications = customers + ";" + peers + ";" + providers;				

				String[] classLevels = classifications.split(";");
				int currentLevel = 0;
				for (int j = 0; j < classLevels.length; j++) {
					if (classLevels[j].isEmpty() || classLevels[j].matches(",+"))
						continue;
					String[] classStrings = classLevels[j].split(",");
					neighborClassificationLists[i][currentLevel] = new long[classStrings.length];
					for (int k = 0; k < neighborClassificationLists[i][currentLevel].length; k++) {
						if (classStrings[k].isEmpty())
							continue;
						neighborClassificationLists[i][currentLevel][k] = Long.parseLong(classStrings[k]);
					}
					currentLevel++;
				}

				long[] neighborList = noZeroes(concatAll(neighborClassificationLists[i]));

				Arrays.sort(neighborList);
				
				String forbiddens = properties.getProperty(prefix + PROP_FORBIDDEN, "");
				if (forbiddens.isEmpty()) {
					neighborForbiddenLists[i] = new long[]{nodeInfos[i].getID()}; //dummy data
				} else {
					String[] forbiddenStrings = forbiddens.split(",");
					neighborForbiddenLists[i] = new long[forbiddenStrings.length];
					for (int j = 0; j < neighborForbiddenLists[i].length; j++) {
						if (forbiddenStrings[j].isEmpty())
							continue;
						neighborForbiddenLists[i][j] = Long.parseLong(forbiddenStrings[j]);
					}
				}
				

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
	protected String writeOutputToFile() throws Exception {
		String fileName = outputFolder + "/" + "idr_output_"
				+ "_round" + String.format("%03d", currentTimeSlot)+ ".txt";
		String output = "";
		for (int i = 0; i < nodeInfos.length; i++) {
			IDRNodeInfo node = nodeInfos[i];
			output += "Route for domain " + node.getID() + ":\n";
			output += "AS_PATH Length: " + finalResult[i][0] + "\n";
			output += "NextHop: " + finalResult[i][1] + "\n\n";
		}

		Services.writeFile(output, fileName);
		return output;
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
	public long[][] getInitialClassificationSharesForPP(int ppNr) {
		long [][] shares = new long[numberOfNodes][];
		for (int i = 0; i < numberOfNodes; i++) {
			shares[i] = initialClassificationShares[i][ppNr];
		}
		return shares;		
	}
	
	/**
	 *  Returns the forbidden shares for each node.
	 * @param ppNr the PP number
	 * @return the shares. Dimensions: [numberOfNodes][M+1]
	 */
	public long[][] getInitialForbiddenSharesForPP(int ppNr) {
		long [][] shares = new long[numberOfNodes][];
		for (int i = 0; i < numberOfNodes; i++) {
			shares[i] = initialForbiddenShares[i][ppNr];
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
	 *  Returns shares of M+1 zeroes for each node.
	 * @param ppNr the PP number
	 * @return the shares. Dimensions: [numberOfNodes][M+1]
	 */
	public long[][] getZeroSharesForPP(int ppNr) {
		long [][] shares = new long[numberOfNodes][];
		for (int i = 0; i < numberOfNodes; i++) {
			shares[i] = initialZeroShares[i][ppNr];
		}
		return shares;		
	}


}
