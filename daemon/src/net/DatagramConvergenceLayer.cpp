/*
 * DatagramConvergenceLayer.cpp
 *
 *  Created on: 21.11.2011
 *      Author: morgenro
 */

#include "net/DatagramConvergenceLayer.h"
#include "net/DatagramConnection.h"

#include "core/BundleCore.h"
#include "core/TimeEvent.h"
#include "core/NodeEvent.h"

#include <ibrcommon/Logger.h>
#include <ibrcommon/thread/MutexLock.h>

#include <string.h>

namespace dtn
{
	namespace net
	{
		DatagramConvergenceLayer::DatagramConvergenceLayer(DatagramService *ds)
		 : _service(ds), _discovery_sn(0)
		{
		}

		DatagramConvergenceLayer::~DatagramConvergenceLayer()
		{
			// wait until all connections are down
			{
				ibrcommon::MutexLock l(_connection_cond);
				for(std::list<DatagramConnection*>::iterator i = _connections.begin(); i != _connections.end(); ++i)
				{
					(*i)->shutdown();
				}

				while (_connections.size() != 0) _connection_cond.wait();
			}

			join();
			delete _service;
		}

		dtn::core::Node::Protocol DatagramConvergenceLayer::getDiscoveryProtocol() const
		{
			return _service->getProtocol();
		}

		void DatagramConvergenceLayer::send(const std::string &destination, const char *buf, int len) throw (DatagramException)
		{
			char tmp[_service->getMaxMessageSize()];

			// add header to the segment
			tmp[0] = HEADER_SEGMENT;
			::memcpy(&tmp[1], buf, len);

			// only on sender at once
			ibrcommon::MutexLock l(_send_lock);

			// forward the send request to DatagramService
			_service->send(destination, &tmp[0], len + 1);
		}

		void DatagramConvergenceLayer::send(const char *buf, int len) throw (DatagramException)
		{
			char tmp[_service->getMaxMessageSize()];

			// add header to the segment
			tmp[0] = HEADER_BROADCAST;
			::memcpy(&tmp[1], buf, len);

			// only on sender at once
			ibrcommon::MutexLock l(_send_lock);

			// forward the send request to DatagramService
			_service->send(&tmp[0], len);
		}

		void DatagramConvergenceLayer::callback_send(DatagramConnection&, const std::string &destination, const char *buf, int len) throw (DatagramException)
		{
			// forward to send method
			send(destination, buf, len);
		}

		void DatagramConvergenceLayer::callback_ack(DatagramConnection&, const std::string &destination, const char *buf, int len) throw (DatagramException)
		{
			char tmp[_service->getMaxMessageSize()];

			// add header to the segment
			tmp[0] = HEADER_ACK;
			::memcpy(&tmp[1], buf, len);

			// only on sender at once
			ibrcommon::MutexLock l(_send_lock);

			// forward the send request to DatagramService
			_service->send(destination, &tmp[0], len + 1);
		}

		void DatagramConvergenceLayer::queue(const dtn::core::Node &node, const ConvergenceLayer::Job &job)
		{
			const std::list<dtn::core::Node::URI> uri_list = node.get(_service->getProtocol());
			if (uri_list.empty()) return;

			// get the first element of the result
			const dtn::core::Node::URI &uri = uri_list.front();

			// some debugging
			IBRCOMMON_LOGGER_DEBUG(10) << "DatagramConvergenceLayer::queue"<< IBRCOMMON_LOGGER_ENDL;

			// lock the connection list while working with it
			ibrcommon::MutexLock lc(_connection_cond);

			// get a new or the existing connection for this address
			DatagramConnection &conn = getConnection( uri.value );

			// queue the job to the connection
			conn.queue(job);
		}

		DatagramConnection& DatagramConvergenceLayer::getConnection(const std::string &identifier)
		{
			// Test if connection for this address already exist
			for(std::list<DatagramConnection*>::iterator i = _connections.begin(); i != _connections.end(); ++i)
			{
				IBRCOMMON_LOGGER_DEBUG(10) << "Connection identifier: " << (*i)->getIdentifier() << IBRCOMMON_LOGGER_ENDL;
				if ((*i)->getIdentifier() == identifier)
					return *(*i);
			}

			// Connection does not exist, create one and put it into the list
			DatagramConnection *connection = new DatagramConnection(identifier, _service->getMaxMessageSize() - 1, (*this));

			_connections.push_back(connection);
			_connection_cond.signal(true);

			IBRCOMMON_LOGGER_DEBUG(10) << "DatagramConvergenceLayer::getConnection "<< connection->getIdentifier() << IBRCOMMON_LOGGER_ENDL;
			connection->start();
			return *connection;
		}

		void DatagramConvergenceLayer::connectionUp(const DatagramConnection *conn)
		{
			IBRCOMMON_LOGGER_DEBUG(10) << "DatagramConvergenceLayer::connectionUp: " << conn->getIdentifier() << IBRCOMMON_LOGGER_ENDL;
		}

		void DatagramConvergenceLayer::connectionDown(const DatagramConnection *conn)
		{
			ibrcommon::MutexLock lc(_connection_cond);

			std::list<DatagramConnection*>::iterator i;
			for(i = _connections.begin(); i != _connections.end(); ++i)
			{
				if ((*i) == conn)
				{
					IBRCOMMON_LOGGER_DEBUG(10) << "DatagramConvergenceLayer::connectionDown: " << conn->getIdentifier() << IBRCOMMON_LOGGER_ENDL;

					_connections.erase(i);
					_connection_cond.signal(true);
					return;
				}
			}
		}

		void DatagramConvergenceLayer::componentUp()
		{
			bindEvent(dtn::core::TimeEvent::className);
			try {
				_service->bind();
			} catch (const std::exception &e) {
				IBRCOMMON_LOGGER_DEBUG(10) << "Failed to add DatagramConvergenceLayer on " << _service->getInterface().toString() << IBRCOMMON_LOGGER_ENDL;
				IBRCOMMON_LOGGER_DEBUG(10) << "Exception: " << e.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void DatagramConvergenceLayer::componentDown()
		{
			unbindEvent(dtn::core::TimeEvent::className);
		}

		void DatagramConvergenceLayer::sendAnnoucement()
		{
			DiscoveryAnnouncement announcement(DiscoveryAnnouncement::DISCO_VERSION_01, dtn::core::BundleCore::local);

			// set sequencenumber
			announcement.setSequencenumber(_discovery_sn);
			_discovery_sn++;

			// clear all services
			announcement.clearServices();

			// serialize announcement
			stringstream ss;
			ss << announcement;

			int len = ss.str().size();

			try {
				send(ss.str().c_str(), len);
			} catch (const DatagramException&) {
				// ignore any send failure
			};
		}

		void DatagramConvergenceLayer::componentRun()
		{
			_running = true;

			size_t maxlen = _service->getMaxMessageSize();
			std::string address;
			char data[maxlen];
			char header = 0;
			size_t len = 0;

			while (_running)
			{
				IBRCOMMON_LOGGER_DEBUG(10) << "DatagramConvergenceLayer::componentRun early" << IBRCOMMON_LOGGER_ENDL;

				try {
					// Receive full frame from socket
					len = _service->recvfrom(data, maxlen, address);
				} catch (const DatagramException&) {
					_running = false;
					break;
				}

				IBRCOMMON_LOGGER_DEBUG(10) << "DatagramConvergenceLayer::componentRun" << IBRCOMMON_LOGGER_ENDL;

				// We got nothing from the socket, keep reading
				if (len == 0) continue;

				// Retrieve header of frame
				header = data[0];

				IBRCOMMON_LOGGER_DEBUG(10) << "DatagramConvergenceLayer::componentRun: ADDRESS " << address << IBRCOMMON_LOGGER_ENDL;

				// Check for extended header and retrieve if available
				if (header & HEADER_BROADCAST)
				{
					IBRCOMMON_LOGGER(notice) << "Received announcement for DatagramConvergenceLayer discovery" << IBRCOMMON_LOGGER_ENDL;
					DiscoveryAnnouncement announce;
					stringstream ss;
					ss.write(data+1, len-1);
					ss >> announce;

					// convert the announcement into NodeEvents
					Node n(announce.getEID());

					// timeout value
					size_t to_value = 30;

					// add
					n.add(Node::URI(Node::NODE_DISCOVERED, _service->getProtocol(), address, to_value));

					const std::list<DiscoveryService> services = announce.getServices();
					for (std::list<DiscoveryService>::const_iterator iter = services.begin(); iter != services.end(); iter++)
					{
						const DiscoveryService &s = (*iter);
						n.add(Node::Attribute(Node::NODE_DISCOVERED, s.getName(), s.getParameters(), to_value));
					}

					// create and raise a new event
					dtn::core::NodeEvent::raise(n, dtn::core::NODE_INFO_UPDATED);

					continue;
				}
				else if ( header & HEADER_SEGMENT )
				{
					ibrcommon::MutexLock lc(_connection_cond);

					// Connection instance for this address
					DatagramConnection& connection = getConnection(address);

					// Decide in which queue to write based on the src address
					connection.queue(data+1, len-1);
				}
				else if ( header & HEADER_ACK )
				{
					ibrcommon::MutexLock lc(_connection_cond);

					// Connection instance for this address
					DatagramConnection& connection = getConnection(address);

					// Decide in which queue to write based on the src address
					connection.ack(data+1, len-1);
				}

				yield();
			}
		}

		void DatagramConvergenceLayer::raiseEvent(const Event *evt)
		{
			try {
				const TimeEvent &time=dynamic_cast<const TimeEvent&>(*evt);
				if (time.getAction() == TIME_SECOND_TICK)
					if (time.getTimestamp() % 5 == 0)
						sendAnnoucement();
			} catch (const std::bad_cast&)
			{}
		}

		bool DatagramConvergenceLayer::__cancellation()
		{
			_running = false;
			_service->shutdown();

			return true;
		}

		const std::string DatagramConvergenceLayer::getName() const
		{
			return "DatagramConvergenceLayer";
		}
	} /* namespace data */
} /* namespace dtn */
