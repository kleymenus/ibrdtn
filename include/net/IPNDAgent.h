/*
 * IPNDAgent.h
 *
 *  Created on: 09.09.2009
 *      Author: morgenro
 *
 * IPND is based on the Internet-Draft for DTN-IPND.
 *
 * MUST support multicast
 * SHOULD support broadcast and unicast
 *
 *
 *
 */

#ifndef IPNDAGENT_H_
#define IPNDAGENT_H_

#include "ibrdtn/config.h"
#include "net/DiscoveryAgent.h"
#include "net/DiscoveryAnnouncement.h"
#include "ibrcommon/net/NetInterface.h"

using namespace dtn::data;

namespace dtn
{
	namespace net
	{
		class IPNDAgent : public DiscoveryAgent
		{
		public:
			IPNDAgent(ibrcommon::NetInterface net);
			virtual ~IPNDAgent();

		protected:
			void send(DiscoveryAnnouncement &announcement);
			void run();

		private:
			ibrcommon::NetInterface _interface;
			int _socket;
		};
	}
}

#endif /* IPNDAGENT_H_ */
