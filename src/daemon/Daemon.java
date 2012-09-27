package daemon;

/**
 * MAJOR ASSUMPTION: nodes are numbered from 1 to Connections, inclusive
 */

import java.io.*;
import java.net.*;
import java.util.Properties;


class Daemon{

	public static final String PORT = "Listen";
	public static final String CONNECTIONS = "Connections"; // specifies number of connected Privacy Peers
	public Thread[] threads;
	public Properties props, defaults;
	public ServerSocket listenSocket;
	public long[] newStore, oldStore;
	public int connections, reportsLeft;
	
	
	
	public Daemon(String args[]) throws Exception {
		/*
		 * Doing properties in a weird way. Each property
		 * could be specified 3 times: The default is
		 * overridden by the config file, is overridden by
		 * the command line.
		 */
		FileReader configFile = null;

		defaults = new Properties();
		defaults.setProperty(PORT, "3488"); // my birthday
		defaults.setProperty(CONNECTIONS, "3"); // Should be overridden
		props = new Properties(defaults);

		if (args.length % 2 != 0){
			throw new Exception("Needs to have an even number of arguments");
		}
		if (args.length > 1){
			if (args[0].equals("-config") || args[0].equals("-c")) {
				configFile = new FileReader(args[1]);
				props.load(configFile);
			}
			for (int i = (configFile == null ? 0 : 2); i < args.length; i += 2){
				if (args[i].charAt(0) == '-') {
					props.setProperty(args[i].substring(1), args[i+1]);
				} else {
					throw new Exception("Parameters look like -Listen <port>");
				}
			}
		} //end if (args.length > 1)

		connections = Integer.parseInt(props.getProperty(CONNECTIONS));
		
		newStore =	new long[connections+1];
		reportsLeft = connections;

		// create server socket
		listenSocket = new ServerSocket(Integer.parseInt(props.getProperty(PORT)));

		threads = new Thread[connections];
	}

	public void go() throws Exception {
		System.out.println("server listening at: "
				+ listenSocket);

		for (int i = 0; i < threads.length; i++) {
			try {
				threads[i] = new Thread(new PrivacyPeerHandler(this));
				threads[i].start();

			} catch (Exception e)
			{}
		} // end of thread creation

		for (int i = 0; i < threads.length; i++){
			threads[i].join();
		}
	}

	/*
	 * Returns true if this was the last report we were waiting for.
	 */
	public boolean report(int nodeID, long nextHop){
		newStore[nodeID] = nextHop;
		reportsLeft--;
		if (reportsLeft <= 0) {
			oldStore = newStore;
			newStore = new long[connections+1];
			reportsLeft = connections;
			this.notifyAll();
			return true;
		}
		return false;
	}
	
	/**
	 * Usage: Daemon -Listen [port] -Connections [# nodes]
	 */
	public static void main(String args[]) throws Exception  {
		Daemon d = new Daemon(args);
		d.go();
	} // end of main

} // end of class 
