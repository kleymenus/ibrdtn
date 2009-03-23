#include "core/UDPConvergenceLayer.h"
#include "data/BundleFactory.h"

#include "utils/Utils.h"
#include <sys/socket.h>
#include <poll.h>
#include <errno.h>

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#include <iostream>


using namespace dtn::data;

namespace dtn
{
	namespace core
	{
		//const int UDPConvergenceLayer::MAX_SIZE = 64000;
		const int UDPConvergenceLayer::DEFAULT_PORT = 4556;

		UDPConvergenceLayer::UDPConvergenceLayer(string bind_addr, unsigned short port, bool broadcast, unsigned int mtu)
			: Service("UDPConvergenceLayer"), m_maxmsgsize(mtu), m_selfaddr(bind_addr), m_selfport(port) //, m_fakeloss(0)
		{
			struct sockaddr_in address;

			address.sin_addr.s_addr = inet_addr(bind_addr.c_str());
		  	address.sin_port = htons(port);

			// Create socket for listening for client connection requests.
			m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

			if (m_socket < 0)
			{
				cerr << "UDPConvergenceLayer: cannot create listen socket" << endl;
				exit(1);
			}

			// enable broadcast
			int b = 1;

			if (broadcast)
			{
				if ( setsockopt(m_socket, SOL_SOCKET, SO_BROADCAST, (char*)&b, sizeof(b)) == -1 )
				{
					cerr << "UDPConvergenceLayer: cannot send broadcasts" << endl;
					exit(1);
				}

				// broadcast addresses should be usable more than once.
				setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&b, sizeof(b));
			}

			address.sin_family = AF_INET;

			if ( bind(m_socket, (struct sockaddr *) &address, sizeof(address)) < 0 )
			{
				cerr << "UDPConvergenceLayer: cannot bind socket" << endl;
			    exit(1);
			}
		}

		UDPConvergenceLayer::~UDPConvergenceLayer()
		{
			MutexLock l(m_writelock);
			close(m_socket);
		}

		TransmitReport UDPConvergenceLayer::transmit(const Bundle &b)
		{
			unsigned int size = b.getLength();

			if (size > m_maxmsgsize)
			{
				// create a fragment
				unsigned int offset = 0;
				Bundle *fragment = BundleFactory::slice(b, m_maxmsgsize, offset);

				// transfer the fragment
				transmit(*fragment);
				delete fragment;

				// create a fragment from the remaining data
				fragment = BundleFactory::slice(b, UINT_MAX, offset);

				// throw exception
				//throw new TransferNotCompletedException(fragment);
				// TODO: return the fragment to the router

				return TRANSMIT_SUCCESSFUL;
			}

			unsigned char *data = b.getData();

			struct sockaddr_in clientAddress;

			// destination parameter
			clientAddress.sin_family = AF_INET;

			// broadcast
			clientAddress.sin_addr.s_addr = inet_addr(m_selfaddr.c_str());
			clientAddress.sin_port = htons(m_selfport);

			MutexLock l(m_writelock);

		    // send converted line back to client.
		    int ret = sendto(m_socket, data, size, 0, (struct sockaddr *) &clientAddress, sizeof(clientAddress));
		    free(data);

		    if (ret < 0)
		    {
		    	return CONVERGENCE_LAYER_BUSY;
		    }

		    return TRANSMIT_SUCCESSFUL;
		}

		TransmitReport UDPConvergenceLayer::transmit(const Bundle &b, const Node &node)
		{
			unsigned int size = b.getLength();

			if (size > m_maxmsgsize)
			{
				// create a fragment
				unsigned int offset = 0;
				Bundle *fragment = BundleFactory::slice(b, m_maxmsgsize, offset);

				// transfer the fragment
				transmit(*fragment, node);
				delete fragment;

				// create a fragment from the remaining data
				fragment = BundleFactory::slice(b, UINT_MAX, offset);

				// throw exception
				//throw new TransferNotCompletedException(fragment);
				// TODO: return the fragment to the router

				return TRANSMIT_SUCCESSFUL;
			}

			unsigned char *data = b.getData();

			struct sockaddr_in clientAddress;

			// destination parameter
			clientAddress.sin_family = AF_INET;

			// use node to define the destination
			clientAddress.sin_addr.s_addr = inet_addr(node.getAddress().c_str());
			clientAddress.sin_port = htons(node.getPort());

			MutexLock l(m_writelock);

		    // Send converted line back to client.
		    int ret = sendto(m_socket, data, size, 0, (struct sockaddr *) &clientAddress, sizeof(clientAddress));
		    free(data);

		    if (ret < 0)
		    {
		    	return CONVERGENCE_LAYER_BUSY;
		    }

		    return TRANSMIT_SUCCESSFUL;
		}

		void UDPConvergenceLayer::tick()
		{
			receiveBundle();
		}

		void UDPConvergenceLayer::receiveBundle()
		{
			struct sockaddr_in clientAddress;
			socklen_t clientAddressLength = sizeof(clientAddress);

			unsigned char data[m_maxmsgsize];

			// first peek if data is incoming, wait max. 100ms
			struct pollfd psock[1];

			psock[0].fd = m_socket;
			psock[0].events = POLLIN;
			psock[0].revents = POLLERR;

			switch (poll(psock, 1, 10))
			{
				case 0:
					// Timeout

				break;

				case -1:
					// Error

				break;

				default:
					// data waiting
				    int len = recvfrom(m_socket, data, m_maxmsgsize, MSG_WAITALL, (struct sockaddr *) &clientAddress, &clientAddressLength);

				    if (len < 0)
				    {
				    	// no data received
				    	return;
				    }

					try {
					    // if we get data, then create a bundle
						BundleFactory &fac = BundleFactory::getInstance();
						Bundle *b = fac.parse(data, len);
					    ConvergenceLayer::eventBundleReceived(*b);
					    delete b;
					} catch (InvalidBundleData ex) {
						cerr << "Received a invalid bundle: " << ex.what() << endl;
					}
				break;
			}
		}
	}
}
