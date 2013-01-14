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

/**
 * stores information about a IDR  (privacy) peer
 * 
 * @author Dilip Many
 * 
 */
public class IDRPeerInfo {

	private String ID;
	private int index;

	private IDRNodeInfo[] nodeInfos;

	/** contains the initial shares */
	private boolean isInitialSharesReceived = false;

	/**
	 * Creates a new IDR info object
	 */
	public IDRPeerInfo(String ID, int index) {
		this.ID = ID;
		this.index = index;
	}

	public String getID() {
		return ID;
	}

	public void setID(String iD) {
		ID = iD;
	}

	public long getNodeID() {
		return Long.parseLong(ID);
	}

	public int getIndex() {
		return index;
	}

	public void setIndex(int index) {
		this.index = index;
	}

	public boolean isInitialSharesReceived() {
		return isInitialSharesReceived;
	}

	public void setInitialSharesReceived(boolean isInitialSharesReceived) {
		this.isInitialSharesReceived = isInitialSharesReceived;
	}

	public void setNodeInfos(IDRNodeInfo[] nodeInfos) {
		this.nodeInfos = nodeInfos;
	}

	public IDRNodeInfo[] getNodeInfos() {
		return nodeInfos;
	}


}
