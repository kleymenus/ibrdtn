#include "config.h"

#include "core/BundleCore.h"
#include "core/AbstractBundleStorage.h"
#include "core/SimpleBundleStorage.h"
#include "core/StaticBundleRouter.h"
#include "core/ConvergenceLayer.h"
#include "core/DummyConvergenceLayer.h"
#include "core/UDPConvergenceLayer.h"
#include "core/TCPConvergenceLayer.h"
#include "core/MultiplexConvergenceLayer.h"
#include "core/Node.h"
#include "core/EventSwitch.h"
#include "core/NodeEvent.h"
#include "core/EventDebugger.h"
#include "core/CustodyManager.h"
using namespace dtn::core;

#ifdef HAVE_LIBSQLITE3
#include "core/SQLiteBundleStorage.h"
#endif

#ifdef USE_EMMA_CODE
#include "emma/GPSProvider.h"
#include "emma/GPSConnector.h"
#include "emma/GPSDummy.h"
#include "emma/EmmaConvergenceLayer.h"
#include "emma/MeasurementWorker.h"
#include "emma/MeasurementJob.h"
using namespace emma;
#endif

#include "daemon/Configuration.h"
#include "daemon/EchoWorker.h"
#include "daemon/TestApplication.h"
using namespace dtn::daemon;

#include "utils/Utils.h"
#include "utils/Service.h"
using namespace dtn::utils;

#include <vector>
#include <iostream>
#include <csignal>
#include <unistd.h>
#include <sys/types.h>

#ifdef DO_DEBUG_OUTPUT
#include "daemon/Debugger.h"
#endif

#ifdef HAVE_LIBLUA5_1
#include "daemon/LuaCore.h"
#endif

bool m_running = true;

void term(int signal)
{
	if (signal >= 1)
	{
		m_running = false;
	}
}

int main(int argc, char *argv[])
{
	// catch process signals
	signal(SIGINT, term);
	signal(SIGTERM, term);

#ifdef HAVE_LIBLUA5_1
	dtn::lua::LuaCore luac;
#endif

	// create a configuration
	Configuration &conf = Configuration::getInstance();

	string configurationfile = "config.ini";

	for (int i = 0; i < argc; i++)
	{
		string arg = argv[i];

		if (arg == "-c" && argc > i)
		{
			configurationfile = argv[i + 1];
		}
	}

	cout << "Configuration: " << configurationfile << endl;

	// load configuration
	ConfigFile _conf(configurationfile);
	conf.setConfigFile(_conf);

	// set user id
	if ( _conf.keyExists("user") )
	{
		cout << "Switching UID to " << _conf.read<unsigned int>( "user", 0 ) << endl;
		setuid( _conf.read<unsigned int>( "user", 0 ) );
	}

	// set group id
	if ( _conf.keyExists("group") )
	{
		cout << "Switching GID to " << _conf.read<unsigned int>( "group", 0 ) << endl;
		setgid( _conf.read<unsigned int>( "group", 0 ) );
	}

#ifdef DO_DEBUG_OUTPUT
	// create event debugger
	EventDebugger eventdebugger;
#endif

	// create the bundle core object
	BundleCore& core = BundleCore::getInstance(conf.getLocalUri());
	core.start();

#ifdef USE_EMMA_CODE
	GPSProvider *gpsprov = NULL;
	GPSConnector *gpsc = NULL;

	if ( conf.useGPSDaemon() )
	{
		gpsc = new GPSConnector( conf.getGPSHost(), conf.getGPSPort() );
		gpsc->start();
		gpsprov = gpsc;
	}
	else
	{
		// create a gps dummy with static values
		pair<double,double> position = conf.getStaticPosition();
		gpsprov = new GPSDummy( position.first, position.second );
	}
#endif

	// create a static router
	StaticBundleRouter router( conf.getStaticRoutes(), conf.getLocalUri() );

	// create multiplex convergence layer
	MultiplexConvergenceLayer multicl;
	core.setConvergenceLayer(&multicl);

	// create a list of static nodes
	vector<Node> static_nodes = conf.getStaticNodes();

	// get names of the convergence layers
	vector<string> netlist = conf.getNetList();
	{
		vector<string>::iterator iter = netlist.begin();

		while (iter != netlist.end())
		{
			string key = (*iter);
			string type = conf.getNetType(key);
			ConvergenceLayer *netcl = NULL;

#ifdef USE_EMMA_CODE
			if (type == "emma")
			{
				cout << "Adding ConvergenceLayer for EMMA (" << conf.getNetInterface(key) << ":" << conf.getNetPort(key) << ")" << endl;
				netcl = new EmmaConvergenceLayer( conf.getLocalUri(), conf.getNetInterface(key), conf.getNetPort(key), conf.getNetBroadcast(key) );
			}
			else
#endif
			if (type == "udp")
			{
				cout << "Adding ConvergenceLayer for UDP (" << conf.getNetInterface(key) << ":" << conf.getNetPort(key) << ")" << endl;
				netcl = new UDPConvergenceLayer( conf.getNetInterface(key), conf.getNetPort(key) );
			}
			else if (type == "tcp")
			{
				cout << "Adding ConvergenceLayer for TCP (" << conf.getNetInterface(key) << ":" << conf.getNetPort(key) << ")" << endl;
				netcl = new TCPConvergenceLayer( conf.getLocalUri(), conf.getNetInterface(key), conf.getNetPort(key) );
			}

			// replace DummyConvergenceLayer in the static nodes with real convergence layers
			if (netcl != NULL)
			{
				// add cl to the multiplex cl
				multicl.add(netcl);

				// load static nodes, replace DummyConvergenceLayer
				// and register at the router
				vector<Node>::iterator iter = static_nodes.begin();

				while (iter != static_nodes.end())
				{
					Node n = (*iter);
					DummyConvergenceLayer *dummy = dynamic_cast<DummyConvergenceLayer*>(n.getConvergenceLayer());

					if (dummy != NULL)
					{
						if (dummy->getName() == key)
						{
							n.setConvergenceLayer(netcl);
							delete dummy;
						}
					}

					(*iter) = n;

					iter++;
				}
			}

			iter++;
		}
	}

#ifdef HAVE_LIBSQLITE3
	AbstractBundleStorage *storage = NULL;

	if ( conf.useSQLiteStorage() )
	{
		storage = new SQLiteBundleStorage(
					conf.getSQLiteDatabase(),
					conf.doSQLiteFlush()
					);
	} else {
		storage = new SimpleBundleStorage(conf.getStorageMaxSize() * 1024, 4096);
	}
#else
	AbstractBundleStorage *storage = new SimpleBundleStorage(conf.getStorageMaxSize() * 1024, 4096);
#endif

#ifdef USE_EMMA_CODE
	MeasurementWorker *mworker = NULL;

	// add service for measurements (EMMA)
	if ( _conf.read<int>( "measurement_jobs", 0 ) > 0 )
	{
		MeasurementWorkerConfig config;

		config.interval = _conf.read<int>( "measurement_interval", 5 );
		config.destination = _conf.read<string>( "measurement_destination", "dtn:none" );
		config.lifetime = _conf.read<int>( "measurement_lifetime", 20 );
		config.custody = _conf.read<int>( "measurement_custody", 0 ) == 1;

		int jobs = _conf.read<int>( "measurement_jobs", 0 );

		for (int i = 0; i < jobs; i++)
		{
			stringstream key1;
			stringstream key2;

			key1 << "measurement_job" << i+1 << "_type";
			key2 << "measurement_job" << i+1 << "_cmd";

			int type = _conf.read<int>( key1.str(), 0 );
			string cmd = _conf.read<string>( key2.str(), "" );

			MeasurementJob job( (unsigned char)type, cmd );
			config.jobs.push_back( job );
		}

		mworker = new MeasurementWorker( config );
	}
#endif

#ifdef DO_DEBUG_OUTPUT
	// Debugger
	Debugger debugger;
#endif

	// add echo module
	EchoWorker echo;

	// init system
	cout << "dtn node ready" << endl;

	// start the services
	router.start();
	multicl.initialize();

	Service *storage_service = dynamic_cast<Service*>(storage);
	if (storage_service != NULL) storage_service->start();

#ifdef USE_EMMA_CODE
	if ( gpsc != NULL )
	{
		gpsc->start();
	}

	if (mworker != NULL)
	{
		mworker->start();
	}
#endif

	// announce static nodes
	{
		vector<Node>::iterator iter = static_nodes.begin();

		while (iter != static_nodes.end())
		{
			// create a event
			EventSwitch::raiseEvent(new NodeEvent( (*iter), dtn::core::NODE_INFO_UPDATED) );
			iter++;
		}
	}

	while (m_running)
	{
		usleep(10000);
	}

#ifdef USE_EMMA_CODE
	if ( gpsc != NULL )
	{
		gpsc->abort();
	}

	if (mworker != NULL)
	{
		mworker->abort();
	}
#endif

	// stop the services
	if (storage_service != NULL) storage_service->abort();

	multicl.terminate();
	router.abort();

#ifdef USE_EMMA_CODE
	delete gpsprov;
	delete mworker;
#endif

	cout << "shutdown dtn node" << endl;

	return 0;
};
