/*
 * IPNDAgent.cpp
 *
 *  Created on: 14.09.2009
 *      Author: morgenro
 */

#include "net/IPNDAgent.h"
#include "core/BundleCore.h"
#include <ibrdtn/data/Exceptions.h>
#include <sstream>
#include <string.h>
#include <ibrcommon/Logger.h>
#include <ibrcommon/net/MulticastSocket.h>
#include <ibrcommon/net/BroadcastSocket.h>
#include "Configuration.h"
#include <typeinfo>

namespace dtn
{
	namespace net
	{
		IPNDAgent::IPNDAgent(int port, std::string address)
		 : DiscoveryAgent(dtn::daemon::Configuration::getInstance().getDiscovery()), _version(DiscoveryAnnouncement::DISCO_VERSION_01), _socket(NULL), _destination(address), _port(port)
		{
			if (ibrcommon::MulticastSocket::isMulticast(_destination))
			{
				IBRCOMMON_LOGGER(info) << "DiscoveryAgent: multicast mode " << address << ":" << port << IBRCOMMON_LOGGER_ENDL;
				_socket = new ibrcommon::MulticastSocket();
			}
			else
			{
				IBRCOMMON_LOGGER(info) << "DiscoveryAgent: broadcast mode " << address << ":" << port << IBRCOMMON_LOGGER_ENDL;
				_socket = new ibrcommon::BroadcastSocket();
			}

			switch (_config.version())
			{
			case 2:
				_version = DiscoveryAnnouncement::DISCO_VERSION_01;
				break;

			case 1:
				_version = DiscoveryAnnouncement::DISCO_VERSION_00;
				break;

			case 0:
				IBRCOMMON_LOGGER(info) << "DiscoveryAgent: DTN2 compatibility mode" << IBRCOMMON_LOGGER_ENDL;
				_version = DiscoveryAnnouncement::DTND_IPDISCOVERY;
				break;
			};
		}

		IPNDAgent::~IPNDAgent()
		{
			delete _socket;
		}

		void IPNDAgent::bind(const ibrcommon::NetInterface &net)
		{
			IBRCOMMON_LOGGER(info) << "DiscoveryAgent: bind to interface " << net.getInterface() << IBRCOMMON_LOGGER_ENDL;
			_interfaces.push_back(net);
		}

		void IPNDAgent::send(ibrcommon::udpsocket::peer &p, const DiscoveryAnnouncement &announcement)
		{
			stringstream ss;
			ss << announcement;

			string data = ss.str();
			p.send(data.c_str(), data.length());
		}

		void IPNDAgent::sendAnnoucement(const u_int16_t &sn, const std::list<DiscoveryService> &services)
		{
			DiscoveryAnnouncement announcement(_version, dtn::core::BundleCore::local);

			// set sequencenumber
			announcement.setSequencenumber(sn);

			if (_sockets.empty())
			{
				if (!_config.shortbeacon())
				{
					// add services
					for (std::list<DiscoveryService>::const_iterator iter = services.begin(); iter != services.end(); iter++)
					{
						const DiscoveryService &service = (*iter);
						announcement.addService(service);
					}
				}

				ibrcommon::udpsocket::peer p = _socket->getPeer(_destination, _port);

				// send announcement
				send(p, announcement);

				return;
			}

			for (std::list<ibrcommon::NetInterface>::const_iterator it_iface = _interfaces.begin(); it_iface != _interfaces.end(); it_iface++)
			{
				const ibrcommon::NetInterface &iface = (*it_iface);

				// clear all services
				announcement.clearServices();

				if (!_config.shortbeacon())
				{
					// add services
					for (std::list<DiscoveryService>::const_iterator iter = services.begin(); iter != services.end(); iter++)
					{
						const DiscoveryService &service = (*iter);
						if (service.onInterface(iface))
						{
							announcement.addService(service);
						}
					}
				}

				ibrcommon::udpsocket *sock = _sockets[iface.getInterface()];

				if (sock == NULL)
				{
					sock = _socket;
				}

				ibrcommon::udpsocket::peer p = sock->getPeer(_destination, _port);

				// send announcement
				send(p, announcement);
			}
		}

		void IPNDAgent::componentUp()
		{
			DiscoveryAgent::componentUp();

			try {
				ibrcommon::MulticastSocket &sock = dynamic_cast<ibrcommon::MulticastSocket&>(*_socket);
				sock.bind(_port);

				if (_interfaces.empty())
				{
					sock.joinGroup(_destination);
				}
				else
				{
					for (std::list<ibrcommon::NetInterface>::const_iterator iter = _interfaces.begin(); iter != _interfaces.end(); iter++)
					{
						const ibrcommon::NetInterface &net = (*iter);

						if (_sockets.empty())
						{
							_sockets[net.getInterface()] = _socket;
							sock.setInterface(net);
						}
						else
						{
							ibrcommon::MulticastSocket *newsock = new ibrcommon::MulticastSocket();
							newsock->setInterface(net);
							_sockets[net.getInterface()] = newsock;
						}

						sock.joinGroup(_destination, (*iter));
					}
				}
			} catch (std::bad_cast) {

			}

			try {
				ibrcommon::BroadcastSocket &sock = dynamic_cast<ibrcommon::BroadcastSocket&>(*_socket);
				sock.bind(_port);
			} catch (std::bad_cast) {

			}
		}

		void IPNDAgent::componentDown()
		{
			_socket->shutdown();
			DiscoveryAgent::componentDown();
		}

		void IPNDAgent::componentRun()
		{
			while (true)
			{
				DiscoveryAnnouncement announce(_version);

				char data[1500];

				std::string sender;
				int len = _socket->receive(data, 1500, sender);
				
				if (announce.isShort())
				{
					// TODO: generate name with the sender address
				}

				if (announce.getServices().empty())
				{
					announce.addService(dtn::net::DiscoveryService("tcpcl", "ip=" + sender + ";port=4556;"));
				}

				if (len < 0) return;

				stringstream ss;
				ss.write(data, len);

				try {
					ss >> announce;
					received(announce);
				} catch (dtn::InvalidDataException ex) {
				} catch (ibrcommon::IOException ex) {
				}

				yield();
			}
		}

		bool IPNDAgent::__cancellation()
		{
			// since this is an receiving thread we have to cancel the hard way
			return false;
		}

		const std::string IPNDAgent::getName() const
		{
			return "IPNDAgent";
		}
	}
}
