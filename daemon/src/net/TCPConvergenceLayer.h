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

#include "net/ConvergenceLayer.h"
#include "net/DiscoveryService.h"
#include "net/DiscoveryServiceProvider.h"

#include <ibrdtn/data/Bundle.h>
#include <ibrdtn/data/EID.h>
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
		class TCPConvergenceLayer;

		class TCPConnection : public StreamConnection::Callback, public ibrcommon::DetachedThread
		{
		public:
			/**
			 * Constructor for a new TCPConnection object.
			 * @param tcpsrv
			 * @param stream TCP stream to talk to the other peer.
			 * @param name
			 * @param timeout
			 * @return
			 */
			TCPConnection(TCPConvergenceLayer &tcpsrv, ibrcommon::tcpstream *stream, const dtn::data::EID &name, const size_t timeout = 10);

			/**
			 * Constructor for a new TCPConnection object.
			 * @param tcpsrv
			 * @param node The node to talk to.
			 * @param name
			 * @param timeout
			 * @return
			 */
			TCPConnection(TCPConvergenceLayer &tcpsrv, const dtn::core::Node &node, const dtn::data::EID &name, const size_t timeout = 10);

			/**
			 * Destructor
			 * @return
			 */
			virtual ~TCPConnection();

			/**
			 * This method is called after accept()
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
			const StreamContactHeader& getHeader() const;

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

			bool match(const dtn::core::Node &n) const;
			bool match(const dtn::data::EID &destination) const;
			bool match(const dtn::core::NodeEvent &evt) const;

			friend TCPConnection& operator>>(TCPConnection &conn, dtn::data::Bundle &bundle);
			friend TCPConnection& operator<<(TCPConnection &conn, const dtn::data::Bundle &bundle);

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

			TCPConvergenceLayer &_callback;
		};

		/**
		 * This class implement a ConvergenceLayer for TCP/IP.
		 * http://tools.ietf.org/html/draft-irtf-dtnrg-tcp-clayer-02
		 */
		class TCPConvergenceLayer : public dtn::daemon::IndependentComponent, public ConvergenceLayer, public DiscoveryServiceProvider, public dtn::core::EventReceiver
		{
			friend class TCPConnection;
		public:
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

			/**
			 * handler for events
			 */
			void raiseEvent(const dtn::core::Event *evt);

			/**
			 * Queue a new transmission job for this convergence layer.
			 * @param job
			 */
			void queue(const dtn::core::Node &n, const ConvergenceLayer::Job &job);

			/**
			 * Open a connection to the given node.
			 * @param n
			 * @return
			 */
			void open(const dtn::core::Node &n);

			/**
			 * @see Component::getName()
			 */
			virtual const std::string getName() const;

			/**
			 * Returns the discovery protocol type
			 * @return
			 */
			dtn::core::Node::Protocol getDiscoveryProtocol() const;

			/**
			 * this method updates the given values
			 */
			void update(std::string &name, std::string &data);
			bool onInterface(const ibrcommon::vinterface &net) const;

		protected:
			bool __cancellation();

			void componentUp();
			void componentRun();
			void componentDown();

		private:
			/**
			 * Add a connection to the connection list.
			 * @param conn
			 */
			void connectionUp(TCPConnection *conn);

			/**
			 * Removes a connection from the connection list. This method is called by the
			 * connection object itself to cleanup on a disconnect.
			 * @param conn
			 */
			void connectionDown(TCPConnection *conn);

			static const int DEFAULT_PORT;
			bool _running;

			ibrcommon::vinterface _net;
			int _port;

			ibrcommon::tcpserver _tcpsrv;

			ibrcommon::Mutex _connections_lock;
			std::list<TCPConnection*> _connections;
		};
	}
}

#endif /* TCPCONVERGENCELAYER_H_ */
