#ifndef LOWPANCONVERGENCELAYER_H_
#define LOWPANCONVERGENCELAYER_H_

#include "Component.h"
#include "net/ConvergenceLayer.h"
#include "net/LOWPANConnection.h"
#include "ibrcommon/Exceptions.h"
#include "net/DiscoveryServiceProvider.h"
#include <ibrcommon/net/vinterface.h>
#include "ibrcommon/net/lowpansocket.h"

#include <list>

using namespace dtn::data;

namespace dtn
{
	namespace net
	{
		/**
		 * This class implement a ConvergenceLayer for LOWPAN.
		 */
		class LOWPANConvergenceLayer : public ConvergenceLayer, public dtn::daemon::IndependentComponent, public DiscoveryServiceProvider
		{
		public:
			LOWPANConvergenceLayer(ibrcommon::vinterface net, int panid, bool broadcast = false, unsigned int mtu = 1280); //MTU is actually 127...

			virtual ~LOWPANConvergenceLayer();

			/**
			 * this method updates the given values
			 */
			void update(const ibrcommon::vinterface &iface, std::string &name, std::string &data) throw(dtn::net::DiscoveryServiceProvider::NoServiceHereException);

			dtn::core::Node::Protocol getDiscoveryProtocol() const;

			void queue(const dtn::core::Node &n, const ConvergenceLayer::Job &job);

			LOWPANConvergenceLayer& operator>>(dtn::data::Bundle&);

			/**
			 * @see Component::getName()
			 */
			virtual const std::string getName() const;

		protected:
			virtual void componentUp();
			virtual void componentRun();
			virtual void componentDown();
			bool __cancellation();

		private:
			ibrcommon::lowpansocket *_socket;

			ibrcommon::vinterface _net;
			int _panid;
			int m_socket;

			//std::list<int> ConnectionList;
			std::list<LOWPANConnection*> ConnectionList;

			static const int DEFAULT_PANID;

			unsigned int m_maxmsgsize;

			ibrcommon::Mutex m_writelock;
			ibrcommon::Mutex m_readlock;

			bool _running;
		};
	}
}
#endif /*LOWPANCONVERGENCELAYER_H_*/
