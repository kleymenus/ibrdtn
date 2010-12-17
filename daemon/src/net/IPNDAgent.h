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

#include "net/DiscoveryAgent.h"
#include "net/DiscoveryAnnouncement.h"
#include <ibrcommon/net/vinterface.h>
#include <ibrcommon/net/udpsocket.h>
#include <list>
#include <map>

using namespace dtn::data;

namespace dtn
{
	namespace net
	{
		class IPNDAgent : public DiscoveryAgent
		{
		public:
			IPNDAgent(int port, std::string address);
			virtual ~IPNDAgent();

			void bind(const ibrcommon::vinterface &net);

			/**
			 * @see Component::getName()
			 */
			virtual const std::string getName() const;

		protected:
			void sendAnnoucement(const u_int16_t &sn, const std::list<DiscoveryService> &services);
			virtual void componentRun();
			virtual void componentUp();
			virtual void componentDown();
			bool __cancellation();

		private:
			void send(ibrcommon::udpsocket::peer &p, const DiscoveryAnnouncement &announcement);

			DiscoveryAnnouncement::DiscoveryVersion _version;
			ibrcommon::udpsocket *_socket;
			ibrcommon::vaddress _destination;
			std::list<ibrcommon::vinterface> _interfaces;

			int _port;
		};
	}
}

#endif /* IPNDAGENT_H_ */
