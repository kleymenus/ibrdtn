/*
 * dtnrecv.cpp
 *
 *  Created on: 06.11.2009
 *      Author: morgenro
 */

#include "ibrdtn/api/Client.h"
#include "ibrdtn/api/FileBundle.h"
#include "ibrdtn/utils/tcpclient.h"
#include "ibrdtn/utils/Mutex.h"
#include "ibrdtn/utils/MutexLock.h"

#include <iostream>

void print_help()
{
	cout << "-- dtnrecv (IBR-DTN) --" << endl;
	cout << "Syntax: dtnrecv [options]"  << endl;
	cout << " <filename>    the file to transfer" << endl;
	cout << "* optional parameters *" << endl;
	cout << " -h|--help          display this text" << endl;
	cout << " --file <filename>  write the incoming data to the a file instead of the standard output" << endl;
	cout << " --name <name>      set the application name (e.g. filetransfer)" << endl;

}

int main(int argc, char *argv[])
{
	string filename = "";
	string name = "filetransfer";
	bool stdout = true;

	for (int i = 0; i < argc; i++)
	{
		string arg = argv[i];

		// print help if requested
		if (arg == "-h" || arg == "--help")
		{
			print_help();
			return 0;
		}

		if (arg == "--name" && argc > i)
		{
			name = argv[i + 1];
		}

		if (arg == "--file" && argc > i)
		{
			filename = argv[i + 1];
			stdout = false;
		}
	}

	// Create a stream to the server using TCP.
	dtn::utils::tcpclient conn("127.0.0.1", 4550);

	// Initiate a client for synchronous receiving
	dtn::api::Client client(name, conn, false);

	// Connect to the server. Actually, this function initiate the
	// stream protocol by starting the thread and sending the contact header.
	client.connect();

	// create a bundle
	dtn::api::Bundle b;

	if (!stdout) cout << "Wait for incoming bundle... ";

	// receive the bundle
	client >> b;

	// Shutdown the client connection.
	client.shutdown();

	conn.close();

	// write the data to output
	if (stdout)
	{
		b.getData().read(cout);
	}
	else
	{
		cout << " received." << endl;
		cout << "Writing bundle payload to " << filename << endl;

		fstream file(filename.c_str(), ios::in|ios::out|ios::binary|ios::trunc);
		b.getData().read(file);
		file.close();

		cout << "finished" << endl;
	}

	return 0;
}