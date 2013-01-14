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

import java.util.concurrent.BrokenBarrierException;
import java.util.concurrent.CyclicBarrier;
import java.util.logging.Level;

import mpc.protocolPrimitives.PrimitivesException;
import services.Services;
import services.Stopper;
import services.Utils;
import connections.PrivacyViolationException;

/**
 * Protocol between a privacy peer and another privacy peer for the topk
 * protocol.
 * 
 * @author Dilip Many
 * 
 */
public class IDRProtocolPrivacyPeerToPP extends IDRProtocol {

	/**
	 * reference to topk privacy peer object that started this protocol
	 * instance
	 */
	protected IDRPrivacyPeer privacyPeer;

	public static final int V1_STEPS = 11;

	/**
	 * Creates a new instance of the topk protocol between two privacy
	 * peers.
	 * 
	 * @param threadNumber
	 *            Protocol's thread number
	 * @param privacyPeer
	 *            the privacy peer instantiating this thread
	 * @param privacyPeerID
	 *            the counterpart privacy peer
	 * @param privacyPeerIndex
	 *            the counterpart privacy peer's index
	 * @param stopper
	 *            Stopper to stop protocol thread
	 * @throws Exception
	 */

	public IDRProtocolPrivacyPeerToPP(int threadNumber, IDRPrivacyPeer privacyPeer, String privacyPeerID,
			int privacyPeerIndex, Stopper stopper) {
		super(threadNumber, privacyPeer, privacyPeerID, privacyPeerIndex, stopper);
		this.privacyPeer = privacyPeer;
	}

	public synchronized void prepareRandom(CyclicBarrier ppThreadsBarrier)
			throws BrokenBarrierException, PrimitivesException, PrivacyViolationException, InterruptedException{
		if (ppThreadsBarrier.await() == 0) {
			privacyPeer.prepareRandom1();
		}
		ppThreadsBarrier.await();
		if (!doOperations()) {
			logger.severe("Error");
			return;
		}
		ppThreadsBarrier.await();

		if (ppThreadsBarrier.await() == 0) {
			privacyPeer.prepareRandom2();
		}
		ppThreadsBarrier.await();
		if (!doOperations()) {
			logger.severe("Error");
			return;
		}
		ppThreadsBarrier.await();

		if (ppThreadsBarrier.await() == 0) {
			privacyPeer.prepareRandom3();
		}
		ppThreadsBarrier.await();
		if (!doOperations()) {
			logger.severe("Error");
			return;
		}
		ppThreadsBarrier.await();
	}

	public synchronized boolean runStep(CyclicBarrier ppThreadsBarrier, boolean useISPs)
			throws BrokenBarrierException, PrimitivesException, PrivacyViolationException, InterruptedException{
		if (privacyPeer.goAhead(useISPs)) {

			if (ppThreadsBarrier.await() == 0) {
				privacyPeer.runStep1(useISPs);
			}
			ppThreadsBarrier.await();
			if (!doOperations()) {
				logger.severe("Error");
				return false;
			}
			ppThreadsBarrier.await();

			if (ppThreadsBarrier.await() == 0) {
				privacyPeer.runStep2(useISPs);
			}
			ppThreadsBarrier.await();
			if (!doOperations()) {
				logger.severe("Error");
				return false;
			}
			ppThreadsBarrier.await();

			if (ppThreadsBarrier.await() == 0) {
				privacyPeer.runStep3(useISPs);
			}
			ppThreadsBarrier.await();
			if (!doOperations()) {
				logger.severe("Error");
				return false;
			}
			ppThreadsBarrier.await();

			if (ppThreadsBarrier.await() == 0) {
				privacyPeer.runStep4(useISPs);
			}
			ppThreadsBarrier.await();
			if (!doOperations()) {
				logger.severe("Error");
				return false;
			}
			ppThreadsBarrier.await();

			if (ppThreadsBarrier.await() == 0) {
				privacyPeer.runStep5(useISPs);
			}
			ppThreadsBarrier.await();
			if (!doOperations()) {
				logger.severe("Error");
				return false;
			}
			ppThreadsBarrier.await();

			if (ppThreadsBarrier.await() == 0) {
				privacyPeer.runStep6(useISPs);
			}
			ppThreadsBarrier.await();
			if (!doOperations()) {
				logger.severe("Error");
				return false;
			}
			ppThreadsBarrier.await();

			if (ppThreadsBarrier.await() == 0) {
				privacyPeer.runStep7(useISPs);
			}
			ppThreadsBarrier.await();
			if (!doOperations()) {
				logger.severe("Error");
				return false;
			}
			ppThreadsBarrier.await();

			if (ppThreadsBarrier.await() == 0) {
				privacyPeer.runStep8(useISPs);
			}
			ppThreadsBarrier.await();
			if (!doOperations()) {
				logger.severe("Error");
				return false;
			}
			ppThreadsBarrier.await();

		}

		if (ppThreadsBarrier.await() == 0) {
			privacyPeer.setAllNextHops(useISPs);
		}
		/*
		ppThreadsBarrier.await();
		if (!doOperations()) {
			logger.severe("Error");
			return false;
		}
		 */
		ppThreadsBarrier.await();

		return true;
	}

	/**
	 * Run the MPC IDR protocol for the privacy peer
	 */
	public synchronized void run() {
		initialize(privacyPeer.getTimeSlotCount(), privacyPeer.getNumberOfItems(), privacyPeer.getNumberOfInputPeers());

		// wait for all shares
		logger.log(Level.INFO, "thread " + Thread.currentThread().getId() + " waits for all shares to arrive...");
		privacyPeer.waitForNextPPProtocolStep();
		if (wasIStopped()) {
			return;
		}

		CyclicBarrier ppThreadsBarrier = privacyPeer.getBarrierPP2PPProtocolThreads();
		try {
			/*
			 * One thread always prepares the data for the next step and then
			 * all threads enter doOperations() and process the operations in
			 * parallel.
			 */	

			/*
			 * Generate random number generator if necessary
			 */
			if (privacyPeer.needRandom()) {
				prepareRandom(ppThreadsBarrier);
			}


			/*
			 * Phase 1, runStep on V1 20 times
			 */
			for (int i = 0; i < V1_STEPS; i++) {
				logger.log(Level.INFO, Services.getFilterPassingLogPrefix() + "Running phase " + (int)(i+1));
				if (!runStep(ppThreadsBarrier, true)) {
					logger.severe("Failure in V1, phase " + (i+1) + ". Returning!");
					return; // exit if something went wrong
				}
			}

			/*
			 * Phase 2, runStep on V2 once
			 */

			logger.log(Level.INFO, Services.getFilterPassingLogPrefix() + "Running phase V2");
			if (!runStep(ppThreadsBarrier, false)) {
				logger.severe("Failure in phase V2. Returning!");
				return; // exit if something went wrong
			}

			// ---------------------------
			// Reconstruct the Final Results
			// ---------------------------

			if (privacyPeer.goAhead(true) || privacyPeer.goAhead(false)) {
				if (ppThreadsBarrier.await() == 0) {

					privacyPeer.scheduleFinalResultReconstruction();
					logger.log(Level.INFO, Services.getFilterPassingLogPrefix() + "Reconstructing final result...");
				}
				ppThreadsBarrier.await();
				if (!doOperations()) {
					logger.severe("Final Result reconstruction failed. returning!");
					return;
				}

				if (ppThreadsBarrier.await() == 0) {
					privacyPeer.finishFinalResultReconstruction();
				}
				privacyPeer.reportTimings();
				/*
				ppThreadsBarrier.await();
				if (!doOperations()) {
					logger.severe("Error");
					return;
				}
				ppThreadsBarrier.await();
				 */
			}

			if (ppThreadsBarrier.await() == 0) {
				privacyPeer.setFinalResult();
			}
		} catch (PrimitivesException e) {
			logger.severe(Utils.getStackTrace(e));
		} catch (InterruptedException e) {
			logger.severe(Utils.getStackTrace(e));
		} catch (BrokenBarrierException e) {
			logger.severe(Utils.getStackTrace(e));
		} catch (PrivacyViolationException e) {
			logger.severe(Utils.getStackTrace(e));
		}
	}
}
