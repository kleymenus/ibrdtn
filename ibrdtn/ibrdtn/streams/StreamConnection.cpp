/*
 * StreamConnection.cpp
 *
 *  Created on: 02.07.2009
 *      Author: morgenro
 */

#include "ibrdtn/streams/StreamConnection.h"
#include "ibrdtn/data/Exceptions.h"
#include "ibrdtn/streams/StreamContactHeader.h"
#include "ibrdtn/streams/StreamDataSegment.h"
#include "ibrdtn/data/Exceptions.h"

using namespace dtn::data;

namespace dtn
{
	namespace streams
	{
		StreamConnection::StreamConnection(StreamConnection::Callback &cb, iostream &stream)
		 : _callback(cb), _ack_size(0), _sent_size(0), _in_state(CONNECTION_INITIAL),
		   _out_state(CONNECTION_INITIAL), _buf(*this, stream), std::iostream(&_buf),
		   _shutdown_reason(CONNECTION_SHUTDOWN_NOTSET)
		{
		}

		StreamConnection::~StreamConnection()
		{
		}

		void StreamConnection::handshake(const dtn::data::EID &eid, const size_t timeout, const char flags)
		{
			try {
				// create a new header
				dtn::streams::StreamContactHeader header(eid);

				// set timeout
				header._keepalive = timeout;

				// set flags
				header._flags = flags;

				// do the handshake
				_peer = _buf.handshake(header);

				// set the stream state
				{
					ibrcommon::MutexLock out_lock(_out_state);
					_out_state.setState(CONNECTION_CONNECTED);

					ibrcommon::MutexLock in_lock(_in_state);
					_in_state.setState(CONNECTION_CONNECTED);
				}

				// signal the complete handshake
				_callback.eventConnectionUp(_peer);
			} catch (dtn::exceptions::InvalidDataException ex) {

			}
		}

		void StreamConnection::close()
		{
			{
				ibrcommon::MutexLock l(_in_state);
				_in_state.setState(CONNECTION_CLOSED);
			}

			{
				ibrcommon::MutexLock l(_out_state);
				_out_state.setState(CONNECTION_CLOSED);
			}
		}

		void StreamConnection::shutdown(ConnectionShutdownCases csc)
		{
			// skip if another shutdown is in progress
			{
				ibrcommon::MutexLock l(_shutdown_reason_lock);
				if (_shutdown_reason != CONNECTION_SHUTDOWN_NOTSET) return;
				_shutdown_reason = csc;
			}

			switch (csc)
			{
				case CONNECTION_SHUTDOWN_IDLE:
					_buf.shutdown(StreamDataSegment::MSG_SHUTDOWN_IDLE_TIMEOUT);
					_callback.eventTimeout();
					break;
				case CONNECTION_SHUTDOWN_ERROR:
					_callback.eventError();
					break;
				case CONNECTION_SHUTDOWN_SIMPLE_SHUTDOWN:
					wait();
					_buf.shutdown(StreamDataSegment::MSG_SHUTDOWN_NONE);
					_callback.eventShutdown();
					break;
				case CONNECTION_SHUTDOWN_NODE_TIMEOUT:
					_callback.eventTimeout();
					break;
				case CONNECTION_SHUTDOWN_PEER_SHUTDOWN:
					_callback.eventShutdown();
					break;
			}

			_buf.close();
			close();
			_callback.eventConnectionDown();
		}

		void StreamConnection::eventShutdown()
		{
			_in_state.setState(CONNECTION_CLOSED);
			_callback.eventShutdown();
		}

		void StreamConnection::eventAck(size_t ack, size_t sent)
		{
			// NOTE: no lock here because it is already set in underflow()
			_ack_size = ack;
			_sent_size = sent;
			_in_state.signal(true);
		}

		bool StreamConnection::isConnected()
		{
			if  (
					(_in_state.getState() > CONNECTION_INITIAL) &&
					(_in_state.getState() < CONNECTION_CLOSED)
				)
					return true;

			return false;
		}

		void StreamConnection::wait()
		{
			ibrcommon::MutexLock l(_in_state);
#ifdef DO_EXTENDED_DEBUG_OUTPUT
			if (_sent_size != _ack_size)
					cout << "StreamConnection::wait(): wait for completion of transmission, current size: " << _ack_size << " of " << _sent_size << endl;
#endif
			while ((_sent_size != _ack_size) && (_in_state.getState() == CONNECTION_CONNECTED))
			{
				_in_state.wait();
#ifdef DO_EXTENDED_DEBUG_OUTPUT
				cout << "StreamConnection::wait(): current size: " << _ack_size << " of " << _sent_size << endl;
#endif
			}

#ifdef DO_EXTENDED_DEBUG_OUTPUT
			std::cout << "StreamConnection::wait(): transfer completed" << std::endl;
#endif
		}

		void StreamConnection::reset()
		{
			ibrcommon::MutexLock l(_in_state);
			_ack_size = 0;
		}

		void StreamConnection::connectionTimeout()
		{
			// connection closed
			{
				ibrcommon::MutexLock l(_in_state);
				_in_state.setState(CONNECTION_CLOSED);
			}

			{
				ibrcommon::MutexLock l(_out_state);
				_out_state.setState(CONNECTION_CLOSED);
			}

			// call superclass
			_callback.eventTimeout();
		}
	}
}