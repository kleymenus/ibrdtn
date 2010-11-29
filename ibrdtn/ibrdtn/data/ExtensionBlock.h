/*
 * ExtensionBlock.h
 *
 *  Created on: 26.05.2010
 *      Author: morgenro
 */

#ifndef EXTENSIONBLOCK_H_
#define EXTENSIONBLOCK_H_

#include "ibrdtn/data/Block.h"
#include <ibrcommon/data/BLOB.h>
#include <map>

namespace dtn
{
	namespace data
	{
		class ExtensionBlock : public Block
		{
		public:
			class Factory
			{
			public:
				virtual dtn::data::Block* create() = 0;

				static Factory& get(char type) throw (ibrcommon::Exception);

			protected:
				Factory(char type);
				virtual ~Factory();

			private:
				static std::map<char, Factory*>& map();

				char _type;
			};

			ExtensionBlock();
			ExtensionBlock(ibrcommon::BLOB::Reference ref);
			virtual ~ExtensionBlock();

			ibrcommon::BLOB::Reference getBLOB();

			virtual size_t getLength() const;
			virtual std::ostream &serialize(std::ostream &stream) const;
			virtual std::istream &deserialize(std::istream &stream);

		private:
			ibrcommon::BLOB::Reference _blobref;
		};
	}
}

#endif /* EXTENSIONBLOCK_H_ */
