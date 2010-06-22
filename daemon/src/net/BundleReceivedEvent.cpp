/*
 * BundleReceivedEvent.cpp
 *
 *  Created on: 15.02.2010
 *      Author: morgenro
 */

#include "net/BundleReceivedEvent.h"
#include "core/BundleCore.h"
#include <ibrcommon/Logger.h>

namespace dtn
{
	namespace net
	{
		BundleReceivedEvent::BundleReceivedEvent(const dtn::data::EID peer, const dtn::data::Bundle &bundle)
		 : _peer(peer), _bundle(bundle)
		{

		}

		BundleReceivedEvent::~BundleReceivedEvent()
		{

		}

		void BundleReceivedEvent::raise(const dtn::data::EID peer, const dtn::data::Bundle &bundle)
		{
			try {
				// store the bundle into a storage module
				dtn::core::BundleCore::getInstance().getStorage().store(bundle);

				// raise the new event
				dtn::core::Event::raiseEvent( new BundleReceivedEvent(peer, bundle) );
			} catch (ibrcommon::IOException ex) {
				IBRCOMMON_LOGGER(notice) << "Unable to store bundle " << bundle.toString() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		const string BundleReceivedEvent::getName() const
		{
			return BundleReceivedEvent::className;
		}

		dtn::data::EID BundleReceivedEvent::getPeer() const
		{
			return _peer;
		}

		dtn::data::MetaBundle BundleReceivedEvent::getBundle() const
		{
			return _bundle;
		}

		string BundleReceivedEvent::toString() const
		{
			return className + ": Bundle received " + _bundle.toString();
		}

		const string BundleReceivedEvent::className = "BundleReceivedEvent";
	}
}
