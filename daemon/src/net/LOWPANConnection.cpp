#include "net/LOWPANConnection.h"
#include "net/BundleReceivedEvent.h"
#include "core/BundleEvent.h"
#include "net/TransferCompletedEvent.h"
#include "net/TransferAbortedEvent.h"
#include "routing/RequeueBundleEvent.h"
#include <ibrcommon/net/vaddress.h>
#include <ibrcommon/net/vinterface.h>
#include "core/BundleCore.h"

#include <ibrcommon/data/BLOB.h>
#include <ibrcommon/Logger.h>
#include <ibrcommon/thread/MutexLock.h>

#include <ibrdtn/data/ScopeControlHopLimitBlock.h>
#include <ibrdtn/utils/Utils.h>
#include <ibrdtn/data/Serializer.h>

#include <poll.h>
#include <errno.h>

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>

#include <iostream>
#include <list>

/* Header:
 * +---------------+
 * |7 6 5 4 3 2 1 0|
 * +---------------+
 * Bit 7-6: 00 to be compatible with 6LoWPAN
 * Bit 5-4: 00 Middle segment
 *	    01 Last segment
 *	    10 First segment
 *	    11 First and last segment
 * Bit 3:   0 Extended frame not available
 *          1 Extended frame available
 * Bit 2-0: sequence number (0-7)
 *
 * Extended header (only if extended frame available)
 * +---------------+
 * |7 6 5 4 3 2 1 0|
 * +---------------+
 * Bit 7:   0 No discovery frame
 *          1 Discovery frame
 * Bit 6-0: Reserved
 *
 * Two bytes at the end of the frame are reserved for the short address of the
 * sender. This is a workaround until recvfrom() gets fixed.
 */

#define SEGMENT_FIRST	0x25
#define SEGMENT_LAST	0x10
#define SEGMENT_BOTH	0x30
#define SEGMENT_MIDDLE	0x00
#define EXTENDED_MASK	0x04

using namespace dtn::data;

namespace dtn
{
	namespace net
	{
		//LOWPANConnection::LOWPANConnection(unsigned short address, LOWPANConvergenceLayer &cl)
		LOWPANConnection::LOWPANConnection(unsigned short address)
		 : address(address)
		{
		}

		LOWPANConnection::~LOWPANConnection()
		{
		}

		lowpanstream& LOWPANConnection::getStream()
		{
			return *_stream;
		}
		void LOWPANConnection::run()
		{
		}

	}
}
