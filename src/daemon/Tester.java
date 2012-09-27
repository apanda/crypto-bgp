package daemon;
import java.io.*;
import java.net.Socket;
public class Tester {

	/**
	 * Usage: java Tester [id] [port]
	 */
	public static void main(String[] args) throws Exception{
		int id = Integer.parseInt(args[0]);
		int port = 3488;
		if (args.length > 1)
			port = Integer.parseInt(args[1]);
		Socket s = new Socket("localhost", port);
		ObjectOutputStream outStream = new ObjectOutputStream(s.getOutputStream());
		ObjectInputStream inStream = new ObjectInputStream(s.getInputStream());
		outStream.writeInt(id);
		outStream.flush();
		for (int i = 0; i < 20; i++) {
			if (id == 3)
				Thread.sleep(1000);
			outStream.writeLong(i * id);
			outStream.flush();
			System.out.print("Writing " + (i * id) + " and got [");
			long[] results = (long[])inStream.readObject();
			for (int j = 0; j < results.length; j++) {
				System.out.print(results[j]);
				if (j + 1 < results.length)
					System.out.print(",");
				else
					System.out.println("]");
			}
		}
		s.close();
	}

}
