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

import java.io.Serializable;

import mpc.MessageBase;

/**
 * message used to exchange data among (privacy) peers in the topk protocol
 * 
 * @author Dilip Many
 * 
 */
public class IDRMessage extends MessageBase implements Serializable {
	private static final long serialVersionUID = 3461683923455914692L;

	/**
	 * Message Type Flags
	 */
	/** indicates if the message contains the initial shares */
	private boolean isInitialSharesMessage = false;
	/** indicates if the message contains the final results */
	private boolean isFinalResultMessage = false;
	
	/** contains all IDRNodeInfos */
	private IDRNodeInfo[] nodeInfos;
	
	/** contains the initial shares */
	//private long[] neighborList = null; // [numberOfNeighbors]
	//private long[] initialPrefShares = null; // [numberOfNeighbors]
	//private long[][] initialExportShares = null; //[numberOfNeighbors][numberOfNeighbors
	
	/** contains the final results */
	private long[] finalResults;
	
	/** is this an ISP? */
	private boolean isp = false;
	
	/** ID of the destination */
	private long destination;


	/**
	 * creates a new IDR protocol message with the specified sender id and
	 * index
	 * 
	 * @param senderID
	 *            the senders id
	 * @param senderIndex
	 *            the senders index
	 */
	public IDRMessage(String senderID, int senderIndex) {
		super(senderID, senderIndex);
	}

	public boolean isInitialSharesMessage() {
		return isInitialSharesMessage;
	}

	public void setIsInitialSharesMessage(boolean isInitialSharesMessage) {
		this.isInitialSharesMessage = isInitialSharesMessage;
	}

	public boolean isFinalResultMessage() {
		return isFinalResultMessage;
	}

	public void setIsFinalResultMessage(boolean isFinalResultMessage) {
		this.isFinalResultMessage = isFinalResultMessage;
	}

	public long[] getFinalResults() {
		return finalResults;
	}

	public void setFinalResults(long[] finalResults) {
		this.finalResults = finalResults;
	}

	public void setIsISP(boolean isp) {
		this.isp = isp;
	}
	
	public boolean isISP() {
		return isp;
	}

	public void setDestination(long destination) {
		this.destination = destination;
	}
	
	public long getDestination() {
		return destination;
	}
	
	public int getNumberOfNodes() {
		return nodeInfos.length;
	}
	
	public void setNodeInfos(IDRNodeInfo[] nodeInfos) {
		this.nodeInfos = nodeInfos;
	}
	
	public IDRNodeInfo[] getNodeInfos() {
		return nodeInfos;
	}
	
	public void setInitialPrefShares(long[][] initialPrefShares) {
		for (int i = 0; i < initialPrefShares.length; i++) {
			nodeInfos[i].setInitialPrefShares(initialPrefShares[i]);
		}
	}
	
	public void setInitialExportShares(long[][][] initialExportShares) {
		for (int i = 0; i < initialExportShares.length; i++) {
			nodeInfos[i].setInitialExportShares(initialExportShares[i]);
		}
	}
	
	public void setInitialMatchesShares(long[][][] initialMatchesShares) {
		for (int i = 0; i < initialMatchesShares.length; i++) {
			nodeInfos[i].setInitialMatchesShares(initialMatchesShares[i]);
		}
	}
	
}
