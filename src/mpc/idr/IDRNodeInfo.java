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
import java.util.NoSuchElementException;
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

	private boolean isp = false;

	// The destination works differently. Its next hop is always itself, and it always
	// exports to all neighbors.
	private boolean destination = false;

	/** contains list of neighbors, sorted by id */
	private long[] neighbors = null;

	/** contains the initial shares */
	private boolean isInitialSharesReceived = false;
	private long[] initialClassificationShares = null; // [numberOfNeighbors]
	private long[] initialForbiddenShares = null; // [numberOfForbiddenNodes]
	private long[][] initialExportShares = null; // [numberOfNeighbors][numberOfNeighbors]

	private long[] route = null; // IMPORTANT: Length is always M
	private long pathLength = IDRBase.M;

	private long finalNextHop = 0;

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
		this.destination = destination;
	}

	public long[] getInitialClassificationShares() {
		return initialClassificationShares;
	}

	public void setInitialClassificationShares(long[] initialClassificationShares) {
		this.initialClassificationShares = initialClassificationShares;
	}

	public long[] getInitialForbiddenShares() {
		return initialForbiddenShares;
	}

	public void setInitialForbiddenShares(long[] initialForbiddenShares) {
		this.initialForbiddenShares = initialForbiddenShares;
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

	public boolean isInitialSharesReceived() {
		return isInitialSharesReceived;
	}

	public void setInitialSharesReceived(boolean isInitialSharesReceived) {
		this.isInitialSharesReceived = isInitialSharesReceived;
	}

	public long[] getRoute(){
		return route;		
	}

	public void setPathLength(long pathLength){
		this.pathLength = pathLength;
	}

	public long getPathLength(){
		return pathLength;
	}

	public void setRoute(long[] route) {
		this.route = route;
		// during initialization, if this is the destination,
		// we need to make sure the route includes this id
		if (destination) {
			route[0] = ID;
		}

	}

	public void addToRoute(long id) {
		if (route == null) {
			throw new NoSuchElementException("No route yet");
		}
		for (int i = route.length - 1; i > 0; i--) {
			route[i] = route[i-1];
		}
		route[0] = id;
		pathLength++; // note: pathLength is a share, but this works anyway
	}

	public long getNextHop() {
		if (route == null) {
			return 0;
		} else if (destination) {
			return route[0]; // the destination's nexthop is itself
		} else {
			return route[1]; // not route[0]; that's this node's ID
		}
	}

	public long getFinalNextHop() {
		return finalNextHop;
	}

	public void setFinalNextHop(long finalNextHop) {
		this.finalNextHop = finalNextHop;
	}

}
