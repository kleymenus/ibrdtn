/*
 * TCPConnection.cpp
 *
 *  Created on: 26.04.2010
 *      Author: morgenro
 */

#include "net/TCPConvergenceLayer.h"
#include "net/BundleReceivedEvent.h"
#include "core/BundleCore.h"
#include "core/GlobalEvent.h"
#include "net/ConnectionEvent.h"
#include "core/BundleEvent.h"
#include "ibrcommon/TimeMeasurement.h"

#include "net/TransferCompletedEvent.h"
#include "net/TransferAbortedEvent.h"

#include <ibrcommon/net/NetInterface.h>
#include <ibrcommon/Logger.h>

#include <iostream>
#include <iomanip>

namespace dtn
{
	namespace net
	{
		/*
		 * class TCPConnection
		 */
		TCPConvergenceLayer::TCPConnection::TCPConnection(GenericServer<TCPConnection> &tcpsrv, ibrcommon::tcpstream *stream)
		 : GenericConnection<TCPConvergenceLayer::TCPConnection>((GenericServer<TCPConvergenceLayer::TCPConnection>&)tcpsrv),
		   _peer(), _node(Node::NODE_CONNECTED), _tcpstream(stream), _stream(*this, *stream), _sender(*this),
		   _receiver(*this), _name(), _timeout(0), _lastack(0), _shutdown(false)
		{
			stream->enableKeepalive();
			_node.setProtocol(Node::CONN_TCPIP);
		}

		TCPConvergenceLayer::TCPConnection::~TCPConnection()
		{
			shutdown();
		}

		void TCPConvergenceLayer::TCPConnection::queue(const dtn::data::Bundle &bundle)
		{
			_sender.push(bundle);
		}

		const StreamContactHeader TCPConvergenceLayer::TCPConnection::getHeader() const
		{
			return _peer;
		}

		void TCPConvergenceLayer::TCPConnection::handshake()
		{
			char flags = 0;

			// enable ACKs and NACKs
			flags |= dtn::streams::StreamContactHeader::REQUEST_ACKNOWLEDGMENTS;
			flags |= dtn::streams::StreamContactHeader::REQUEST_NEGATIVE_ACKNOWLEDGMENTS;

			// do the handshake
			_stream.handshake(_name, _timeout, flags);

			// start the sender for outgoing bundles
			_sender.start();
		}

		void TCPConvergenceLayer::TCPConnection::initialize(const dtn::data::EID &name, const size_t timeout)
		{
			_name = name;
			_timeout = timeout;

			// start the receiver for incoming bundles + handshake
			_receiver.start();
		}

		void TCPConvergenceLayer::TCPConnection::eventConnectionUp(const StreamContactHeader &header)
		{
			_peer = header;
			_node.setURI(header._localeid.getString());

			// raise up event
			ConnectionEvent::raise(ConnectionEvent::CONNECTION_UP, _node);
		}

		void TCPConvergenceLayer::TCPConnection::eventConnectionDown()
		{
			// event
			ConnectionEvent::raise(ConnectionEvent::CONNECTION_DOWN, _node);

			try {
				while (true)
				{
					const dtn::data::Bundle bundle = _sentqueue.frontpop();

					if (_lastack > 0)
					{
						// some data are already acknowledged, make a fragment?
						//TODO: make a fragment
						TransferAbortedEvent::raise(EID(_node.getURI()), bundle, dtn::net::TransferAbortedEvent::REASON_CONNECTION_DOWN);
					}
					else
					{
						// raise transfer abort event for all bundles without an ACK
						TransferAbortedEvent::raise(EID(_node.getURI()), bundle, dtn::net::TransferAbortedEvent::REASON_CONNECTION_DOWN);
					}

					// set last ack to zero
					_lastack = 0;
				}
			} catch (ibrcommon::Exception ex) {
				// queue emtpy
			}

			free();
		}

		void TCPConvergenceLayer::TCPConnection::eventBundleRefused()
		{
			try {
				const dtn::data::Bundle bundle = _sentqueue.frontpop();

				// requeue the bundle
				TransferAbortedEvent::raise(EID(_node.getURI()), bundle, dtn::net::TransferAbortedEvent::REASON_REFUSED);

				// set ACK to zero
				_lastack = 0;

			} catch (ibrcommon::Exception ex) {
				// pop on empty queue!
			}
		}

		void TCPConvergenceLayer::TCPConnection::eventBundleForwarded()
		{
			try {
				const dtn::data::Bundle bundle = _sentqueue.frontpop();

				// signal completion of the transfer
				TransferCompletedEvent::raise(EID(_node.getURI()), bundle);

				// raise bundle event
				dtn::core::BundleEvent::raise(bundle, BUNDLE_FORWARDED);

				// set ACK to zero
				_lastack = 0;
			} catch (ibrcommon::Exception ex) {
				// pop on empty queue!
			}
		}

		void TCPConvergenceLayer::TCPConnection::eventBundleAck(size_t ack)
		{
			_lastack = ack;
		}

		void TCPConvergenceLayer::TCPConnection::eventShutdown()
		{
			// close the tcpstream
			try {
				//_tcpstream->done();
				_tcpstream->close();
			} catch (ibrcommon::ConnectionClosedException ex) {

			}
		}

		void TCPConvergenceLayer::TCPConnection::eventTimeout()
		{
			// event
			ConnectionEvent::raise(ConnectionEvent::CONNECTION_TIMEOUT, _node);

			// close the tcpstream
			try {
				//_tcpstream->done();
				_tcpstream->close();
			} catch (ibrcommon::ConnectionClosedException ex) {

			}
		}

		void TCPConvergenceLayer::TCPConnection::eventError()
		{
			// close the tcpstream
			try {
				_tcpstream->close();
			} catch (ibrcommon::ConnectionClosedException ex) {

			}
		}

		void TCPConvergenceLayer::TCPConnection::shutdown()
		{
			_stream.closeBuffer();

			// stop the sender
			_sender.shutdown();

			// stop the receiver
			_receiver.shutdown();
		}

		const dtn::core::Node& TCPConvergenceLayer::TCPConnection::getNode() const
		{
			return _node;
		}

		void TCPConvergenceLayer::TCPConnection::rejectTransmission()
		{
			_stream.reject();
		}

		TCPConvergenceLayer::TCPConnection& operator>>(TCPConvergenceLayer::TCPConnection &conn, dtn::data::Bundle &bundle)
		{
			dtn::data::DefaultDeserializer(conn._stream, dtn::core::BundleCore::getInstance()) >> bundle;
			return conn;
		}

		TCPConvergenceLayer::TCPConnection& operator<<(TCPConvergenceLayer::TCPConnection &conn, const dtn::data::Bundle &bundle)
		{
			// prepare a measurement
			ibrcommon::TimeMeasurement m;

			// create a serializer
			dtn::data::DefaultSerializer serializer(conn._stream);

			// put the bundle into the sentqueue
			conn._sentqueue.push(bundle);

			// start the measurement
			m.start();

			try {
				// transmit the bundle
				serializer << bundle;

				// flush the stream
				conn._stream << std::flush;

				// stop the time measurement
				m.stop();

				// get throughput
				double kbytes_per_second = (serializer.getLength(bundle) / m.getSeconds()) / 1024;

				// print out throughput
				IBRCOMMON_LOGGER_DEBUG(5) << "transfer finished after " << m << " with "
						<< std::setiosflags(std::ios::fixed) << std::setprecision(2) << kbytes_per_second << " kb/s" << IBRCOMMON_LOGGER_ENDL;

			} catch (ibrcommon::Exception ex) {
				// the connection not available
				IBRCOMMON_LOGGER_DEBUG(10) << "connection error: " << ex.what() << IBRCOMMON_LOGGER_ENDL;

				// forward exception
				throw;
			}

			return conn;
		}

		TCPConvergenceLayer::TCPConnection::Receiver::Receiver(TCPConnection &connection)
		 : _connection(connection)
		{
		}

		TCPConvergenceLayer::TCPConnection::Receiver::~Receiver()
		{
			JoinableThread::join();
		}

		void TCPConvergenceLayer::TCPConnection::Receiver::run()
		{
			try {
				// firstly, do the handshake
				_connection.handshake();

				while (true)
				{
					dtn::data::Bundle bundle;

					try {
						_connection >> bundle;

						// check the bundle
						if ( ( bundle._destination == EID() ) || ( bundle._source == EID() ) )
						{
							// invalid bundle!
						}
						else
						{
							// raise default bundle received event
							dtn::net::BundleReceivedEvent::raise(EID(), bundle);
						}

					} catch (dtn::data::Validator::RejectedException ex) {
						// bundle rejected
						_connection.rejectTransmission();
					}

					yield();
				}
			} catch (dtn::InvalidDataException ex) {
				// cannot decode the bundle data, reject bundle
				_connection.rejectTransmission();
			} catch (std::exception) { }
		}

		void TCPConvergenceLayer::TCPConnection::Receiver::finally()
		{
			// shutdown the connection if nobody did this before
			_connection._stream.shutdown(StreamConnection::CONNECTION_SHUTDOWN_NODE_TIMEOUT);
		}

		void TCPConvergenceLayer::TCPConnection::Receiver::shutdown()
		{
			JoinableThread::stop();
		}

		TCPConvergenceLayer::TCPConnection::Sender::Sender(TCPConnection &connection)
		 : _connection(connection)
		{
		}

		TCPConvergenceLayer::TCPConnection::Sender::~Sender()
		{
			JoinableThread::join();
		}

		void TCPConvergenceLayer::TCPConnection::Sender::run()
		{
			try {
				while (true)
				{
					dtn::data::Bundle bundle = blockingpop();

					// send bundle
					_connection << bundle;

					// idle a little bit
					yield();
				}
			} catch (std::exception ex) {
				IBRCOMMON_LOGGER(error) << "TCPConnection::Sender terminated by exception: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void TCPConvergenceLayer::TCPConnection::Sender::shutdown()
		{
			unblock();
			JoinableThread::stop();
		}

		void TCPConvergenceLayer::TCPConnection::Sender::finally()
		{
		}
	}
}
