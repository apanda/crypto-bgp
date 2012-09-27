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
/**
 * stores information about a IDR  (privacy) peer
 * 
 * @author Dilip Many
 * 
 */
public class IDRNodeInfo implements Serializable{
	
	static final long serialVersionUID = 2;

	private String peerID;
	private long ID;

	private long nextHop = -1;
	private boolean isp = false;

	// The destination works differently. Its next hop is always itself, and it always
	// exports to all neighbors.
	private boolean destination = false;

	/** contains list of neighbors, sorted by id */
	private long[] neighbors = null;
	
	/** contains the initial shares */
	private boolean isInitialSharesReceived = false;
	private long[] initialPrefShares = null; // [numberOfNeighbors]
	private long[][] initialExportShares = null; // [numberOfNeighbors][numberOfNeighbors]
	private long[][] initialMatchesShares = null; // [numberOfNeighbors][numberOfNeighbors]

	/**
	 * Creates a new IDR info object
	 */
	public IDRNodeInfo(long ID, String peerID) {
		this.ID = ID;
		this.peerID = peerID;
	}

	public long getID() {
		return ID;
	}

	public void setID(long ID) {
		this.ID = ID;
	}

	public String getPeerID() {
		return peerID;
	}

	public void setPeerID(String peerID) {
		this.peerID = peerID;
	}	
	
	public boolean isISP() {
		return isp;
	}

	public void setISP(boolean isp) {
		this.isp = isp;
	}

	public boolean isDestination() {
		return destination;
	}

	public void setDestination(boolean destination) {
		if (destination == false) {
			this.destination = false;
			return;
		} else {
			this.destination = true;
			this.nextHop = ID;
		}
	}

	public long[] getInitialPrefShares() {
		return initialPrefShares;
	}

	public void setInitialPrefShares(long[] initialPrefShares) {
		this.initialPrefShares = initialPrefShares;
	}

	public long[] getNeighbors() {
		return neighbors;
	}

	public void setNeighbors(long[] neighbors) {
		this.neighbors = neighbors;
	}
	
	public long[][] getInitialExportShares() {
		return initialExportShares;
	}

	public void setInitialExportShares(long[][] initialExportShares) {
		this.initialExportShares = initialExportShares;
	}
	
	public long[][] getInitialMatchesShares() {
		return initialMatchesShares;
	}

	public void setInitialMatchesShares(long[][] initialMatchesShares) {
		this.initialMatchesShares = initialMatchesShares;
	}

	public boolean isInitialSharesReceived() {
		return isInitialSharesReceived;
	}

	public void setInitialSharesReceived(boolean isInitialSharesReceived) {
		this.isInitialSharesReceived = isInitialSharesReceived;
	}

	public long getNextHop() {
		return nextHop;
	}

	public void setNextHop(long nextHop) {
		this.nextHop = nextHop;
	}

}
