/*
 * RetransmissionExtension.cpp
 *
 *  Created on: 09.03.2010
 *      Author: morgenro
 */

#include "routing/RetransmissionExtension.h"
#include "core/TimeEvent.h"
#include "routing/RequeueBundleEvent.h"
#include "net/TransferAbortedEvent.h"
#include "net/TransferCompletedEvent.h"
#include "core/BundleExpiredEvent.h"
#include "ibrdtn/utils/Utils.h"
#include "ibrdtn/data/Exceptions.h"

namespace dtn
{
	namespace routing
	{
		RetransmissionExtension::RetransmissionExtension()
		{
		}

		RetransmissionExtension::~RetransmissionExtension()
		{
		}

		void RetransmissionExtension::notify(const dtn::core::Event *evt)
		{
			const dtn::core::TimeEvent *time = dynamic_cast<const dtn::core::TimeEvent*>(evt);
			const dtn::net::TransferCompletedEvent *completed = dynamic_cast<const dtn::net::TransferCompletedEvent*>(evt);
			const dtn::routing::RequeueBundleEvent *requeue = dynamic_cast<const dtn::routing::RequeueBundleEvent*>(evt);
			const dtn::core::BundleExpiredEvent *expired = dynamic_cast<const dtn::core::BundleExpiredEvent*>(evt);

			if (time != NULL)
			{
				if (!_queue.empty())
				{
					const RetransmissionData &data = _queue.front();

					if ( data.getTimestamp() <= time->getTimestamp() )
					{
						try {
							// retransmit the bundle
							getRouter()->transferTo(data.destination, data);
						} catch (dtn::exceptions::NoBundleFoundException ex) {
							// bundle is not available, stop retransmission.
							dtn::net::TransferAbortedEvent::raise(data.destination, data);
						}

						// remove the item off the queue
						_queue.pop();
					}
				}
			}
			else if (completed != NULL)
			{
				// remove the bundleid in our list
				RetransmissionData data(completed->getBundle(), completed->getPeer());
				_set.erase(data);
			}
			else if (requeue != NULL)
			{
				const RetransmissionData data(requeue->_bundle, requeue->_peer);
				std::set<RetransmissionData>::const_iterator iter = _set.find(data);

				if (iter != _set.end())
				{
					// increment the retry counter
					RetransmissionData data2 = (*iter);
					data2++;

					if (data2.getCount() > 10)
					{
						// requeue the bundle
						_set.insert(data2);
						_queue.push(data2);
					}
					else
					{
						// remove the item
						_set.erase(data);

						dtn::net::TransferAbortedEvent::raise(requeue->_peer, requeue->_bundle);
					}
				}
				else
				{
					// queue the bundle
					_set.insert(data);
					_queue.push(data);
				}
			}
		}

		bool RetransmissionExtension::RetransmissionData::operator!=(const RetransmissionData &obj)
		{
			const dtn::data::BundleID &id1 = dynamic_cast<const dtn::data::BundleID&>(obj);
			const dtn::data::BundleID &id2 = dynamic_cast<const dtn::data::BundleID&>(*this);

			if (id1 != id2) return true;
			if (obj.destination != destination) return true;

			return false;
		}

		bool RetransmissionExtension::RetransmissionData::operator==(const RetransmissionData &obj)
		{
			const dtn::data::BundleID &id1 = dynamic_cast<const dtn::data::BundleID&>(obj);
			const dtn::data::BundleID &id2 = dynamic_cast<const dtn::data::BundleID&>(*this);

			if (id1 != id2) return false;
			if (obj.destination != destination) return false;

			return true;
		}

		const size_t RetransmissionExtension::RetransmissionData::getCount() const
		{
			return _count;
		}

		const size_t RetransmissionExtension::RetransmissionData::getTimestamp() const
		{
			return _timestamp;
		}

		RetransmissionExtension::RetransmissionData& RetransmissionExtension::RetransmissionData::operator++(int value)
		{
			_count+=value;
			_timestamp = dtn::utils::Utils::get_current_dtn_time();
			_timestamp += retry;
		}

		RetransmissionExtension::RetransmissionData& RetransmissionExtension::RetransmissionData::operator++()
		{
			_count++;
			_timestamp = dtn::utils::Utils::get_current_dtn_time();
			_timestamp += retry;
		}

		RetransmissionExtension::RetransmissionData::RetransmissionData(const dtn::data::BundleID &id, dtn::data::EID d, size_t r)
		 : dtn::data::BundleID(id), destination(d), _timestamp(0), _count(0), retry(r)
		{
			(*this)++;
		}

		RetransmissionExtension::RetransmissionData::~RetransmissionData()
		{
		}
	}
}
