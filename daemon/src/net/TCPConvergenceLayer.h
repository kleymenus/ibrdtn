/*
 * TCPConvergenceLayer.h
 *
 *  Created on: 05.08.2009
 *      Author: morgenro
 */

#ifndef TCPCONVERGENCELAYER_H_
#define TCPCONVERGENCELAYER_H_

#include "Component.h"

#include "core/EventReceiver.h"
#include "core/NodeEvent.h"
#include "core/Node.h"

#include "net/GenericServer.h"
#include "net/ConvergenceLayer.h"
#include "net/DiscoveryService.h"
#include "net/DiscoveryServiceProvider.h"

#include <ibrdtn/data/Bundle.h>
#include <ibrdtn/data/MetaBundle.h>
#include <ibrdtn/data/Serializer.h>
#include <ibrdtn/streams/StreamConnection.h>
#include <ibrdtn/streams/StreamContactHeader.h>

#include <ibrcommon/net/tcpserver.h>
#include <ibrcommon/net/tcpstream.h>
#include <ibrcommon/net/vinterface.h>

#include <memory>
#include <ibrcommon/thread/Queue.h>

using namespace dtn::streams;

namespace dtn
{
	namespace net
	{
		/**
		 * This class implement a ConvergenceLayer for TCP/IP.
		 * http://tools.ietf.org/html/draft-irtf-dtnrg-tcp-clayer-02
		 */
		class TCPConvergenceLayer : public dtn::daemon::Component, public ConvergenceLayer, public DiscoveryServiceProvider
		{
		public:
			class TCPConnection : public GenericConnection<TCPConvergenceLayer::TCPConnection>, public StreamConnection::Callback, public ibrcommon::DetachedThread
			{
			public:
				TCPConnection(GenericServer<TCPConnection> &tcpsrv, ibrcommon::tcpstream *stream, const dtn::data::EID &name, const size_t timeout = 10);

				virtual ~TCPConnection();

				/**
				 * This method is called by the GenericServer thread after accept()
				 */
				virtual void initialize();

				/**
				 * shutdown the whole tcp connection
				 */
				void shutdown();

				/**
				 * Get the header of this connection
				 * @return
				 */
				const StreamContactHeader getHeader() const;

				/**
				 * Get the associated Node object
				 * @return
				 */
				const dtn::core::Node& getNode() const;

				/**
				 * callback methods for tcpstream
				 */
				virtual void eventShutdown(StreamConnection::ConnectionShutdownCases csc);
				virtual void eventTimeout();
				virtual void eventError();
				virtual void eventConnectionUp(const StreamContactHeader &header);
				virtual void eventConnectionDown();

				virtual void eventBundleRefused();
				virtual void eventBundleForwarded();
				virtual void eventBundleAck(size_t ack);

				dtn::core::Node::Protocol getDiscoveryProtocol() const;

				/**
				 * queue a bundle for this connection
				 * @param bundle
				 */
				void queue(const dtn::data::BundleID &bundle);

				friend TCPConvergenceLayer::TCPConnection& operator>>(TCPConvergenceLayer::TCPConnection &conn, dtn::data::Bundle &bundle);
				friend TCPConvergenceLayer::TCPConnection& operator<<(TCPConvergenceLayer::TCPConnection &conn, const dtn::data::Bundle &bundle);

			protected:
				void rejectTransmission();

				void run();
				void finally();

				void clearQueue();

				void keepalive();

			private:
				class Sender : public ibrcommon::JoinableThread, public ibrcommon::Queue<dtn::data::BundleID>
				{
				public:
					Sender(TCPConnection &connection, size_t &keepalive_timeout);
					virtual ~Sender();

				protected:
					void run();
					void finally();
					bool __cancellation();

				private:
					bool _abort;
					TCPConnection &_connection;
					size_t &_keepalive_timeout;
					dtn::data::BundleID _current_transfer;
				};

				StreamContactHeader _peer;
				dtn::core::Node _node;

				std::auto_ptr<ibrcommon::tcpstream> _tcpstream;
				StreamConnection _stream;

				// This thread gets awaiting bundles of the queue
				// and transmit them to the peer.
				Sender _sender;

				// handshake variables
				dtn::data::EID _name;
				size_t _timeout;

				ibrcommon::Queue<dtn::data::MetaBundle> _sentqueue;
				size_t _lastack;
				size_t _keepalive_timeout;
			};

			class Server : public dtn::net::GenericServer<TCPConvergenceLayer::TCPConnection>, public dtn::core::EventReceiver
			{
			public:
				Server(const ibrcommon::vinterface &net, int port);
				virtual ~Server();

				/**
				 * Queue a new transmission job for this convergence layer.
				 * @param job
				 */
				void queue(const dtn::core::Node &n, const ConvergenceLayer::Job &job);

				/**
				 * handler for events
				 */
				void raiseEvent(const dtn::core::Event *evt);

				/**
				 * @see Component::getName()
				 */
				virtual const std::string getName() const;

			protected:
				TCPConvergenceLayer::TCPConnection* accept();
				void listen();
				void shutdown();

				void connectionUp(TCPConvergenceLayer::TCPConnection *conn);
				void connectionDown(TCPConvergenceLayer::TCPConnection *conn);

			private:
				class Connection
				{
				public:
					Connection(TCPConvergenceLayer::TCPConnection *conn, const dtn::core::Node &node, const bool &active = false);
					~Connection();

					bool match(const dtn::data::EID &destination) const;
					bool match(const dtn::core::NodeEvent &evt) const;

					TCPConvergenceLayer::TCPConnection& operator*();

					TCPConvergenceLayer::TCPConnection *_connection;
					dtn::core::Node _peer;
					bool _active;
				};

				ibrcommon::tcpserver _tcpsrv;
				std::list<Connection> _connections;
			};

			/**
			 * Constructor
			 * @param[in] bind_addr The address to bind.
			 * @param[in] port The port to use.
			 */
			TCPConvergenceLayer(const ibrcommon::vinterface &net, int port);

			/**
			 * Destructor
			 */
			virtual ~TCPConvergenceLayer();

			dtn::core::Node::Protocol getDiscoveryProtocol() const;

			void initialize();
			void startup();
			void terminate();

			/**
			 * this method updates the given values
			 */
			void update(std::string &name, std::string &data);
			bool onInterface(const ibrcommon::vinterface &net) const;

			/**
			 * Queue a new transmission job for this convergence layer.
			 * @param job
			 */
			void queue(const dtn::core::Node &n, const ConvergenceLayer::Job &job);

			/**
			 * @see Component::getName()
			 */
			virtual const std::string getName() const;

		private:
			static const int DEFAULT_PORT;
			bool _running;

			ibrcommon::vinterface _net;
			int _port;
			TCPConvergenceLayer::Server _server;
		};
	}
}

#endif /* TCPCONVERGENCELAYER_H_ */
