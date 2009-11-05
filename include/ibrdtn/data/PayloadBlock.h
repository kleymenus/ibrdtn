/*
 * PayloadBlock.h
 *
 *  Created on: 29.05.2009
 *      Author: morgenro
 */

#include "ibrdtn/default.h"

#ifndef PAYLOADBLOCK_H_
#define PAYLOADBLOCK_H_

#include "ibrdtn/streams/BundleWriter.h"
#include "ibrdtn/data/Block.h"

using namespace dtn::blob;

namespace dtn
{
	namespace data
	{
		class PayloadBlock : public Block
		{
		public:
			static const unsigned char BLOCK_TYPE = 1;

			PayloadBlock();
			PayloadBlock(blob::BLOBManager::BLOB_TYPE type);
			PayloadBlock(BLOBReference ref);
			virtual ~PayloadBlock();

			virtual void read() {};
			virtual void commit() {};
		};
	}
}

#endif /* PAYLOADBLOCK_H_ */