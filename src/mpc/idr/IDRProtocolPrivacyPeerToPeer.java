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

import java.util.logging.Level;

import services.Stopper;
import services.Utils;
import connections.InputPeerConnectionManager;
import connections.PrivacyViolationException;

/**
 * Protocol between a privacy peer and a peer for the topk protocol.
 *
 * @author Dilip Many
 *
 */
public class IDRProtocolPrivacyPeerToPeer extends IDRProtocol {

	/** reference to topk privacy peer object that started this protocol instance */
	protected IDRPrivacyPeer privacyPeer;


	/**
	 * Creates a new instance of a protocol between a privacy peer and a peer.
	 *
	 * @param threadNumber				Protocol's thread number
	 * @param connection				Connection to send messages over
	 * @param myPeerID					Peer's ID
	 * @param myPeerIndex				Peer's index
	 * @param sendFirst					Is this privacy peer first sending, then receiving
	 * @param privacyPeer				Privacy Peer who started the protocol
	 * @param stopper					Stopper to stop protocol thread
	 * @throws Exception
	 */
	public IDRProtocolPrivacyPeerToPeer(int threadNumber, IDRPrivacyPeer privacyPeer, String inputPeerId, int inputPeerIndex, Stopper stopper)  {
		super(threadNumber, privacyPeer, inputPeerId, inputPeerIndex, stopper);
		this.privacyPeer = privacyPeer;
	}


	/**
	 * Run the MPC topk computation protocol for the peer.
	 */
	public void run() {
		initialize(privacyPeer.getTimeSlotCount(), privacyPeer.getNumberOfItems(), privacyPeer.getNumberOfInputPeers());

		// Receive shares from peers
		logger.log(Level.INFO, "Waiting for initial shares from peers...");
		try {
			receiveMessage();
		} catch (PrivacyViolationException e) {
			logger.severe(Utils.getStackTrace(e));
			return;
		}

		// wait for final result
		privacyPeer.waitForNextPeerProtocolStep();
		if(wasIStopped()) {
			return;
		}

		// send final result
		try {
			sendFinalResult();
		} catch (PrivacyViolationException e) {
			logger.severe(Utils.getStackTrace(e));
			return;
		}
	}


	/**
	 * sends the final result to the connected peer
	 *
	 * @return	true if result was sent successfully
	 * @throws PrivacyViolationException 
	 */ 
	protected void sendFinalResult() throws PrivacyViolationException {
		messageToSend = new IDRMessage(myPeerID, myPeerIndex);
		messageToSend.setIsFinalResultMessage(true);
		messageToSend.setFinalResults(privacyPeer.getFinalResult(otherPeerIndex));
		sendMessage();
		
		privacyPeer.finalResultIsSent();

	}
}
