package daemon;
import java.io.*;
import java.net.Socket;
import java.net.SocketException;


public class PrivacyPeerHandler implements Runnable {

	private Daemon daemon;
	private Socket connSocket;
	ObjectInputStream inFromClient;
	ObjectOutputStream outToClient;
	int nodeID = -1;

	public PrivacyPeerHandler(Daemon d) {
		this.daemon = d;
		this.connSocket = null;
	}

	@Override
	public void run() {
		try {
			// take a ready connection from the accepted queue
			synchronized(daemon.listenSocket){
				connSocket = daemon.listenSocket.accept();
			} //synchronized
			System.out.println("receive request from " + connSocket);
			outToClient =
					new ObjectOutputStream(connSocket.getOutputStream());
			//outToClient.flush();
			inFromClient =
					new ObjectInputStream(connSocket.getInputStream());
		} catch (Exception e) {}

		try {
			nodeID = inFromClient.readInt();
			System.out.println("nodeID = " + nodeID);
		} catch (IOException e) {
			System.err.println("Got exception reading ID; dying...");
			e.printStackTrace();
			return;
		}

		while (true) {
			long nextHop;
			// read input from PrivacyPeer and hope it's a valid nextHop
			try {
				nextHop = inFromClient.readLong();
			} catch (EOFException e) {
				System.out.println("Socket closed; aborting...");
				return;
			} catch (IOException e) {
				System.err.println("Got exception reading nextHop from ID " + nodeID + "; using -1...");
				e.printStackTrace();
				nextHop = -1;
			}

			// report this nextHop to the daemon, and wait on other reports if necessary
			synchronized (daemon) {
				if (!daemon.report(nodeID, nextHop)){
					try {
						daemon.wait();
					} catch (InterruptedException ie) {}
				}
			}
			try {
				outToClient.writeObject(daemon.oldStore);
				outToClient.flush();
			} catch (SocketException e) {
				System.out.println("Socket closed; aborting...");
				return;
			} catch (IOException e) {
				System.err.println("Got exception writing nextHop from ID " + nodeID + "; ignoring...");
				e.printStackTrace();
			}
		}
	}

}
