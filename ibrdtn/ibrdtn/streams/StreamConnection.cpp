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
#include <ibrcommon/TimeMeasurement.h>
#include <ibrcommon/Logger.h>

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
		}

		void StreamConnection::reject()
		{
			_buf.reject();
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

			try {
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
			} catch (StreamConnection::StreamErrorException ex) {
				_callback.eventError();
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

		void StreamConnection::wait(const size_t timeout)
		{
			ibrcommon::MutexLock l(_in_state);
			ibrcommon::TimeMeasurement tm;

			if (_sent_size != _ack_size)
			{
				IBRCOMMON_LOGGER_DEBUG(15) << "StreamConnection::wait(): wait for completion of transmission, current size: " << _ack_size << " of " << _sent_size << IBRCOMMON_LOGGER_ENDL;
			}

			if (timeout == 0) tm.start();

			while (!_buf.isCompleted() && (_in_state.getState() == CONNECTION_CONNECTED))
			{
				if (timeout == 0)
				{
					_in_state.wait();
				}
				else
				{
					_in_state.wait(timeout);
					tm.stop();
					if (tm.getMilliseconds() >= timeout)
					{
						IBRCOMMON_LOGGER_DEBUG(15) << "StreamConnection::wait(): transfer aborted (timeout): " << _ack_size << " of " << _sent_size << IBRCOMMON_LOGGER_ENDL;
						return;
					}
				}
				IBRCOMMON_LOGGER_DEBUG(15) << "StreamConnection::wait(): current size: " << _ack_size << " of " << _sent_size << IBRCOMMON_LOGGER_ENDL;
			}

			if (_buf.isCompleted())
			{
				IBRCOMMON_LOGGER_DEBUG(15) << "StreamConnection::wait(): transfer completed" << IBRCOMMON_LOGGER_ENDL;
			}
			else
			{
				IBRCOMMON_LOGGER_DEBUG(15) << "StreamConnection::wait(): transfer aborted" << IBRCOMMON_LOGGER_ENDL;
				throw dtn::ConnectionInterruptedException(_ack_size);
			}
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
