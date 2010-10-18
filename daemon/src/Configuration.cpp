#include "config.h"
#include "Configuration.h"
#include "ibrdtn/utils/Utils.h"
#include "core/Node.h"
#include "ibrcommon/net/NetInterface.h"
#include <ibrcommon/Logger.h>
#include "net/DiscoveryAnnouncement.h"

using namespace dtn::net;
using namespace dtn::core;
using namespace dtn::utils;
using namespace ibrcommon;

namespace dtn
{
	namespace daemon
	{
		Configuration::NetConfig::NetConfig(std::string n, NetType t, const ibrcommon::NetInterface &i, int p, bool d)
		 : name(n), type(t), interface(i), port(p), discovery(d)
		{
		}

		Configuration::NetConfig::NetConfig(std::string n, NetType t, const std::string &a, int p, bool d)
		 : name(n), type(t), interface("lo"), address(a), port(p), discovery(d)
		{
		}

		Configuration::NetConfig::~NetConfig()
		{
		}

		std::string Configuration::version()
		{
			std::stringstream ss;
			ss << PACKAGE_VERSION;
		#ifdef SVN_REVISION
			ss << " (build " << SVN_REVISION << ")";
		#endif

			return ss.str();
		}

		Configuration::Configuration()
		 : _filename("config.ini"), _doapi(true)
		{
		}

		Configuration::~Configuration()
		{}

		Configuration::Discovery::Discovery()
		 : _enabled(true), _timeout(5) {};

		Configuration::Statistic::Statistic() {};

		Configuration::Debug::Debug()
		 : _enabled(false), _quiet(false), _level(0) {};

		Configuration::Logger::Logger()
		 : _quiet(false), _options(0) {};

		Configuration::Network::Network()
		 : _routing("default"), _forwarding(true), _tcp_nodelay(true), _tcp_chunksize(1024), _default_net("lo"), _use_default_net(false) {};

		Configuration::Discovery::~Discovery() {};
		Configuration::Statistic::~Statistic() {};
		Configuration::Debug::~Debug() {};
		Configuration::Logger::~Logger() {};
		Configuration::Network::~Network() {};

		const Configuration::Discovery& Configuration::getDiscovery() const
		{
			return _disco;
		}

		const Configuration::Statistic& Configuration::getStatistic() const
		{
			return _stats;
		}

		const Configuration::Debug& Configuration::getDebug() const
		{
			return _debug;
		}

		const Configuration::Logger& Configuration::getLogger() const
		{
			return _logger;
		}

		const Configuration::Network& Configuration::getNetwork() const
		{
			return _network;
		}

		Configuration& Configuration::getInstance()
		{
			static Configuration conf;
			return conf;
		}

		void Configuration::params(int argc, char *argv[])
		{
			for (int i = 0; i < argc; i++)
			{
				std::string arg = argv[i];

				if (arg == "-c" && argc > i)
				{
						_filename = argv[i + 1];
				}

				if (arg == "-i" && argc > i)
				{
						_network._default_net = ibrcommon::NetInterface(argv[i + 1]);
						_network._use_default_net = true;
				}

				if (arg == "--noapi")
				{
						_doapi = false;
				}

				if ((arg == "--version") || (arg == "-v"))
				{
						std::cout << "IBR-DTN version: " << version() << std::endl;
						exit(0);
				}

				if (arg == "--nodiscovery")
				{
					_disco._enabled = false;
				}

				if (arg == "-d")
				{
					_debug._enabled = true;
					_debug._level = atoi(argv[i + 1]);
				}

				if (arg == "-q")
				{
					_debug._quiet = true;
				}

				if ((arg == "--help") || (arg == "-h"))
				{
						std::cout << "IBR-DTN version: " << version() << std::endl;
						std::cout << "Syntax: dtnd [options]"  << std::endl;
						std::cout << " -h|--help       display this text" << std::endl;
						std::cout << " -c <file>       set a configuration file" << std::endl;
						std::cout << " -i <interface>  interface to bind on (e.g. eth0)" << std::endl;
						std::cout << " -d <level>      enable debugging and set a verbose level" << std::endl;
						std::cout << " -q              enables the quiet mode (no logging to the console)" << std::endl;
						std::cout << " --noapi         disable API module" << std::endl;
						std::cout << " --nodiscovery   disable discovery module" << std::endl;
						exit(0);
				}
			}
		}

		void Configuration::load()
		{
			load(_filename);
		}

		void Configuration::load(string filename)
		{
			try {
				// load main configuration
				_conf = ibrcommon::ConfigFile(filename);

				IBRCOMMON_LOGGER(info) << "Configuration: " << filename << IBRCOMMON_LOGGER_ENDL;
			} catch (ibrcommon::ConfigFile::file_not_found ex) {
				IBRCOMMON_LOGGER(info) << "Using default settings. Call with --help for options." << IBRCOMMON_LOGGER_ENDL;
				_conf = ConfigFile();
			}

			// load all configuration extensions
			_disco.load(_conf);
			_stats.load(_conf);
			_debug.load(_conf);
			_logger.load(_conf);
			_network.load(_conf);
		}

		void Configuration::Discovery::load(const ibrcommon::ConfigFile &conf)
		{
			_timeout = conf.read<unsigned int>("discovery_timeout", 5);
		}

		void Configuration::Statistic::load(const ibrcommon::ConfigFile&)
		{
		}

		void Configuration::Logger::load(const ibrcommon::ConfigFile&)
		{
		}

		void Configuration::Debug::load(const ibrcommon::ConfigFile&)
		{
		}

		bool Configuration::Debug::quiet() const
		{
			return _quiet;
		}

		bool Configuration::Debug::enabled() const
		{
			return _enabled;
		}

		int Configuration::Debug::level() const
		{
			return _level;
		}

		string Configuration::getNodename()
		{
			try {
				return _conf.read<string>("local_uri");
			} catch (ibrcommon::ConfigFile::key_not_found ex) {
				char *hostname_array = new char[64];
				if ( gethostname(hostname_array, 64) != 0 )
				{
					// error
					delete[] hostname_array;
					return "local";
				}

				string hostname(hostname_array);
				delete[] hostname_array;

				stringstream ss;
				ss << "dtn://" << hostname;
				ss >> hostname;

				return hostname;
			}
		}

		const std::list<Configuration::NetConfig>& Configuration::Network::getInterfaces() const
		{
			return _interfaces;
		}

		std::string Configuration::Discovery::address() const throw (ParameterNotFoundException)
		{
			try {
				return Configuration::getInstance()._conf.read<string>("discovery_address");
			} catch (ConfigFile::key_not_found ex) {
				throw ParameterNotFoundException();
			}
		}

		int Configuration::Discovery::port() const
		{
			return Configuration::getInstance()._conf.read<int>("discovery_port", 4551);
		}

		unsigned int Configuration::Discovery::timeout() const
		{
			return _timeout;
		}

		Configuration::NetConfig Configuration::getAPIInterface()
		{
//			try {
				return Configuration::NetConfig("local", Configuration::NetConfig::NETWORK_TCP, ibrcommon::NetInterface("lo"), 4550);
//			} catch (const ConfigFile::key_not_found&) {
//				throw ParameterNotSetException();
//			}
		}

		ibrcommon::File Configuration::getAPISocket()
		{
			try {
				return ibrcommon::File(_conf.read<std::string>("api_socket"));
			} catch (const ConfigFile::key_not_found&) {
				throw ParameterNotSetException();
			}
		}

		std::string Configuration::getStorage() const
		{
			return _conf.read<std::string>("storage", "default");
		}

		void Configuration::Network::load(const ibrcommon::ConfigFile &conf)
		{
			/**
			 * Load static routes
			 */
			_static_routes.clear();

			string key = "route1";
			unsigned int keynumber = 1;

			while (conf.keyExists( key ))
			{
				vector<string> route = dtn::utils::Utils::tokenize(" ", conf.read<string>(key, "dtn:none dtn:none"));
				_static_routes.push_back( dtn::routing::StaticRoutingExtension::StaticRoute( route.front(), route.back() ) );

				keynumber++;
				stringstream ss; ss << "route" << keynumber; ss >> key;
			}

			/**
			 * load static nodes
			 */
			// read the node count
			int count = 1;

			// initial prefix
			std::string prefix = "static1_";

			while ( conf.keyExists(prefix + "uri") )
			{
				Node n(Node::NODE_PERMANENT);

				n.setAddress( conf.read<std::string>(prefix + "address", "127.0.0.1") );
				n.setPort( conf.read<unsigned int>(prefix + "port", 4556) );
				n.setURI( conf.read<std::string>(prefix + "uri", "dtn:none") );

				std::string protocol = conf.read<std::string>(prefix + "proto", "tcp");
				if (protocol == "tcp") n.setProtocol(Node::CONN_TCPIP);
				if (protocol == "udp") n.setProtocol(Node::CONN_UDPIP);
				if (protocol == "zigbee") n.setProtocol(Node::CONN_ZIGBEE);
				if (protocol == "bluetooth") n.setProtocol(Node::CONN_BLUETOOTH);
				if (protocol == "http") n.setProtocol(Node::CONN_HTTP);

				count++;

				std::stringstream prefix_stream;
				prefix_stream << "static" << count << "_";
				prefix = prefix_stream.str();

				_nodes.push_back(n);
			}

			/**
			 * get routing extension
			 */
			_routing = conf.read<string>("routing", "default");

			/**
			 * get the routing extension
			 */
			_forwarding = (conf.read<std::string>("routing_forwarding", "yes") == "yes");

			/**
			 * get network interfaces
			 */
			_interfaces.clear();

			if (_use_default_net)
			{
				_interfaces.push_back( Configuration::NetConfig("default", Configuration::NetConfig::NETWORK_TCP, _default_net, 4556) );
			}
			else try
			{
				vector<string> nets = dtn::utils::Utils::tokenize(" ", conf.read<string>("net_interfaces") );
				for (vector<string>::const_iterator iter = nets.begin(); iter != nets.end(); iter++)
				{
					std::string netname = (*iter);

					std::string key_type = "net_" + netname + "_type";
					std::string key_port = "net_" + netname + "_port";
					std::string key_interface = "net_" + netname + "_interface";
					std::string key_address = "net_" + netname + "_address";
					std::string key_discovery = "net_" + netname + "_discovery";

					std::string type_name = conf.read<string>(key_type, "tcp");
					Configuration::NetConfig::NetType type = Configuration::NetConfig::NETWORK_UNKNOWN;

					if (type_name == "tcp") type = Configuration::NetConfig::NETWORK_TCP;
					if (type_name == "udp") type = Configuration::NetConfig::NETWORK_UDP;
					if (type_name == "http") type = Configuration::NetConfig::NETWORK_HTTP;
					if (type_name == "lowpan") type = Configuration::NetConfig::NETWORK_LOWPAN;

					switch (type)
					{
						case Configuration::NetConfig::NETWORK_HTTP:
						{
							Configuration::NetConfig::NetConfig nc(netname, type,
									conf.read<std::string>(key_address, "http://localhost/"), 0,
									conf.read<std::string>(key_discovery, "yes") == "no");

							_interfaces.push_back(nc);
							break;
						}

						default:
						{
							Configuration::NetConfig::NetConfig nc(netname, type,
									ibrcommon::NetInterface(conf.read<std::string>(key_interface, "lo")),
									conf.read<unsigned int>(key_port, 4556),
									conf.read<std::string>(key_discovery, "yes") == "yes");

							_interfaces.push_back(nc);
							break;
						}
					}
				}
			} catch (ConfigFile::key_not_found ex) {
				// stop the one network is not found.
			}

			/**
			 * TCP options
			 */
			_tcp_nodelay = (conf.read<std::string>("tcp_nodelay", "yes") == "yes");
			_tcp_chunksize = conf.read<unsigned int>("tcp_chunksize", 1024);
		}

		const list<dtn::routing::StaticRoutingExtension::StaticRoute>& Configuration::Network::getStaticRoutes() const
		{
			return _static_routes;
		}

		const std::list<Node>& Configuration::Network::getStaticNodes() const
		{
			return _nodes;
		}

		int Configuration::getTimezone()
		{
			return _conf.read<int>( "timezone", 0 );
		}

		ibrcommon::File Configuration::getPath(string name)
		{
			stringstream ss;
			ss << name << "_path";
			string key; ss >> key;

			try {
				return ibrcommon::File(_conf.read<string>(key));
			} catch (ConfigFile::key_not_found ex) {
				throw ParameterNotSetException();
			}
		}

		unsigned int Configuration::getUID()
		{
			try {
				return _conf.read<unsigned int>("user");
			} catch (ConfigFile::key_not_found ex) {
				throw ParameterNotSetException();
			}
		}

		unsigned int Configuration::getGID()
		{
			try {
				return _conf.read<unsigned int>("group");
			} catch (ConfigFile::key_not_found ex) {
				throw ParameterNotSetException();
			}
		}


		bool Configuration::Discovery::enabled() const
		{
			return _enabled;
		}

		bool Configuration::Discovery::announce() const
		{
			return (Configuration::getInstance()._conf.read<int>("discovery_announce", 1) == 1);
		}

		bool Configuration::Discovery::shortbeacon() const
		{
			return (Configuration::getInstance()._conf.read<int>("discovery_short", 0) == 1);
		}

		char Configuration::Discovery::version() const
		{
			return Configuration::getInstance()._conf.read<int>("discovery_version", 2);
		}

		bool Configuration::doAPI()
		{
			return _doapi;
		}

		string Configuration::getNotifyCommand()
		{
			try {
				return _conf.read<string>("notify_cmd");
			} catch (ConfigFile::key_not_found ex) {
				throw ParameterNotSetException();
			}
		}

		Configuration::RoutingExtension Configuration::Network::getRoutingExtension() const
		{
			if ( _routing == "epidemic" ) return EPIDEMIC_ROUTING;
			if ( _routing == "flooding" ) return FLOOD_ROUTING;
			return DEFAULT_ROUTING;
		}


		bool Configuration::Network::doForwarding() const
		{
			return _forwarding;
		}

		bool Configuration::Network::getTCPOptionNoDelay() const
		{
			return _tcp_nodelay;
		}

		size_t Configuration::Network::getTCPChunkSize() const
		{
			return _tcp_chunksize;
		}

		bool Configuration::Statistic::enabled() const
		{
			return Configuration::getInstance()._conf.keyExists("statistic_type");
		}

		ibrcommon::File Configuration::Statistic::logfile() const throw (ParameterNotSetException)
		{
			try {
				return ibrcommon::File(Configuration::getInstance()._conf.read<std::string>("statistic_file"));
			} catch (ConfigFile::key_not_found ex) {
				throw ParameterNotSetException();
			}
		}

		std::string Configuration::Statistic::type() const
		{
			return Configuration::getInstance()._conf.read<std::string>("statistic_type", "stdout");
		}

		unsigned int Configuration::Statistic::interval() const
		{
			return Configuration::getInstance()._conf.read<unsigned int>("statistic_interval", 300);
		}

		std::string Configuration::Statistic::address() const
		{
			return Configuration::getInstance()._conf.read<std::string>("statistic_address", "127.0.0.1");
		}

		unsigned int Configuration::Statistic::port() const
		{
			return Configuration::getInstance()._conf.read<unsigned int>("statistic_port", 1234);
		}

		size_t Configuration::getLimit(std::string suffix)
		{
			std::string unparsed = _conf.read<std::string>("limit_" + suffix, "0");

			std::stringstream ss(unparsed);

			float value; ss >> value;
			char multiplier = 0; ss >> multiplier;

			switch (multiplier)
			{
			default:
				return (size_t)value;
				break;

			case 'G':
				return (size_t)(value * 1000000000);
				break;

			case 'M':
				return (size_t)(value * 1000000);
				break;

			case 'K':
				return (size_t)(value * 1000);
				break;
			}

			return 0;
		}

		bool Configuration::Logger::quiet() const
		{
			return _quiet;
		}

		unsigned int Configuration::Logger::options() const
		{
			return _options;
		}

		std::ostream& Configuration::Logger::output() const
		{
			return std::cout;
		}
	}
}
