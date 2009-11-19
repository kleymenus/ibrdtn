/* 
 * File:   dtnfiletransfer.cpp
 * Author: morgenro
 *
 * Created on 19. November 2009, 08:21
 */

#include <stdlib.h>

#include "ibrdtn/default.h"
#include "ibrdtn/api/Client.h"
#include "ibrdtn/api/FileBundle.h"
#include "ibrdtn/utils/tcpclient.h"
#include "ibrdtn/utils/Mutex.h"
#include "ibrdtn/utils/MutexLock.h"

#include <iostream>
#include <map>
#include <vector>

class appstream : public std::streambuf
{
public:
    enum { BUF_SIZE = 512 };

    appstream(string command)
     : m_buf( BUF_SIZE+1 )
    {
        setp( &m_buf[0], &m_buf[0] + (m_buf.size()-1) );

        // execute the command
        _handle = popen(command.c_str(), "w");
    }

    virtual ~appstream()
    {
        // close the handle
        pclose(_handle);
    }

protected:
    virtual int sync()
    {
        if( pptr() > pbase() )
        {
            // get the size of the data
            size_t size = pptr() - pbase();

            // write the data
            fwrite(pbase(), size, 1, _handle);

            // mark the buffer as free
            setp( &m_buf[0], &m_buf[0] + (m_buf.size()-1) );
        }
        return 0; // 0 := Ok
    }

private:
    std::vector< char_type > m_buf;
    FILE *_handle;
};

class filereceiver : public dtn::api::Client
{
    public:
        filereceiver(string app, string inbox, string address = "127.0.0.1", int port = 4550)
        : _tcpclient(address, port), dtn::api::Client(app, *this), _inbox(inbox)
        { };

        /**
         * Destructor of the connection.
         */
        virtual ~filereceiver()
        {
                dtn::api::Client::shutdown();

                // Close the tcp connection.
                _tcpclient.close();
        };

    private:
        // file descriptor for the tun device
        string _inbox;

        /**
         * In this API bundles are received asynchronous. To receive bundles it is necessary
         * to overload the Client::received()-method. This will be call on a incoming bundles
         * by another thread.
         */
        void received(dtn::api::Bundle &b)
        {
            dtn::blob::BLOBReference ref = b.getData();

            stringstream cmdstream; cmdstream << "tar -C " << _inbox;

            // create a tar handler
            appstream extractor(cmdstream.str());
            ostream stream(&extractor);

            // write the payload to the extractor
            ref.read( stream );

            // flush the stream
            stream.flush();
        }

        dtn::utils::tcpclient _tcpclient;
};

void print_help()
{
	cout << "-- dtnfiletransfer (IBR-DTN) --" << endl;
	cout << "Syntax: dtnfiletransfer [options] <name> <inbox> <outbox> <destination>"  << endl;
        cout << " <name>           the application name" << endl;
	cout << " <inbox>          directory where incoming files should be placed" << endl;
        cout << " <outbox>         directory with outgoing files" << endl;
        cout << " <destination>    the destination EID for all outgoing files" << endl;
	cout << "* optional parameters *" << endl;
	cout << " -h|--help        display this text" << endl;
        cout << " -w|--workdir     temporary work directory" << endl;
}

map<string,string> readconfiguration(int argc, char** argv)
{
    // print help if not enough parameters are set
    if (argc < 5) { print_help(); exit(0); }

    map<string,string> ret;

    ret["name"] = argv[argc - 4];
    ret["inbox"] = argv[argc - 3];
    ret["outbox"] = argv[argc - 2];
    ret["destination"] = argv[argc - 1];

    for (int i = 0; i < (argc - 4); i++)
    {
        string arg = argv[i];

        // print help if requested
        if (arg == "-h" || arg == "--help")
        {
            print_help();
            exit(0);
        }

        if ((arg == "-w" || arg == "--workdir") && (argc > i))
        {
            ret["workdir"] = argv[i + 1];
        }
    }

    return ret;
}

// set this variable to false to stop the app
bool _running = true;

void term(int signal)
{
    if (signal >= 1)
    {
        _running = false;
    }
}

/*
 * 
 */
int main(int argc, char** argv)
{
    // catch process signals
    signal(SIGINT, term);
    signal(SIGTERM, term);

    // read the configuration
    map<string,string> conf = readconfiguration(argc, argv);
    
    // init working directory
    if (conf.find("workdir") != conf.end())
    {
        dtn::blob::BLOBManager::init(conf["workdir"]);
    }

//    // loop, if no stop if requested
//    while (_running)
//    {
        
        try {
            cout << "init" << endl;

            // Initiate a client for synchronous receiving
            filereceiver client(conf["name"], conf["inbox"]);

            cout << "connect" << endl;

            // Connect to the server. Actually, this function initiate the
            // stream protocol by starting the thread and sending the contact header.
            client.connect();

            // check outbox for files

            // send file in outbox

            // check the connection
            if (!client.isConnected())
            {
                cout << "not connected" << endl;
                // wait some seconds
                sleep(5);

                // and restart
            }

            sleep(1);
            
        } catch (...) {
            cout << "error" << endl;
        }
//    }

    return (EXIT_SUCCESS);
}

