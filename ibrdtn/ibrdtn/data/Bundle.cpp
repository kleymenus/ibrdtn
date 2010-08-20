/*
 * Bundle.cpp
 *
 *  Created on: 29.05.2009
 *      Author: morgenro
 */

#include "ibrdtn/data/Bundle.h"
#include "ibrdtn/data/StatusReportBlock.h"
#include "ibrdtn/data/CustodySignalBlock.h"
#include "ibrdtn/data/ExtensionBlock.h"
#include "ibrdtn/data/ExtensionBlockFactory.h"
#include "ibrdtn/data/Serializer.h"

namespace dtn
{
	namespace data
	{
		std::map<char, ExtensionBlockFactory*>& Bundle::getExtensionBlockFactories()
		{
			static std::map<char, ExtensionBlockFactory*> factories;
			return factories;
		}

		Bundle::Bundle()
		{
		}

		Bundle::~Bundle()
		{
			clearBlocks();
		}

		Bundle::BlockList::BlockList()
		{
		}

		Bundle::BlockList::~BlockList()
		{
		}

		void Bundle::BlockList::push_front(Block *block)
		{
			if (_blocks.empty())
			{
				// set the last block flag
				block->set(dtn::data::Block::LAST_BLOCK, true);
			}

			_blocks.push_front(refcnt_ptr<Block>(block));
		}

		void Bundle::BlockList::push_back(Block *block)
		{
			// set the last block flag
			block->set(dtn::data::Block::LAST_BLOCK, true);

			if (!_blocks.empty())
			{
				// remove the last block flag of the previous block
				dtn::data::Block *lastblock = (_blocks.back().getPointer());
				lastblock->set(dtn::data::Block::LAST_BLOCK, false);
			}

			_blocks.push_back(refcnt_ptr<Block>(block));
		}

		void Bundle::BlockList::insert(Block *block, const Block *before)
		{
			for (std::list<refcnt_ptr<Block> >::iterator iter = _blocks.begin(); iter != _blocks.end(); iter++)
			{
				const dtn::data::Block *lb = (*iter).getPointer();

				if (lb == before)
				{
					_blocks.insert(iter, refcnt_ptr<Block>(block) );
					return;
				}
			}
		}

		void Bundle::BlockList::remove(const Block *block)
		{
			// delete all blocks
			for (std::list<refcnt_ptr<Block> >::iterator iter = _blocks.begin(); iter != _blocks.end(); iter++)
			{
				const dtn::data::Block &lb = (*(*iter));
				if ( &lb == block )
				{
					_blocks.erase(iter++);

					// set the last block bit
					if (!_blocks.empty())
						(*_blocks.back()).set(dtn::data::Block::LAST_BLOCK, true);

					return;
				}
			}
		}

		void Bundle::BlockList::clear()
		{
			// clear the list of objects
			_blocks.clear();
		}

		const std::list<const Block*> Bundle::BlockList::getList() const
		{
			std::list<const dtn::data::Block*> ret;

			for (std::list<refcnt_ptr<Block> >::const_iterator iter = _blocks.begin(); iter != _blocks.end(); iter++)
			{
				ret.push_back( (*iter).getPointer() );
			}

			return ret;
		}

		const std::set<dtn::data::EID> Bundle::BlockList::getEIDs() const
		{
			std::set<dtn::data::EID> ret;

			for (std::list<refcnt_ptr<Block> >::const_iterator iter = _blocks.begin(); iter != _blocks.end(); iter++)
			{
				std::list<EID> elist = (*iter)->getEIDList();

				for (std::list<dtn::data::EID>::const_iterator iter = elist.begin(); iter != elist.end(); iter++)
				{
					ret.insert(*iter);
				}
			}

			return ret;
		}

		bool Bundle::operator!=(const Bundle& other) const
		{
			return PrimaryBlock(*this) != other;
		}

		bool Bundle::operator==(const Bundle& other) const
		{
			return PrimaryBlock(*this) == other;
		}

		bool Bundle::operator<(const Bundle& other) const
		{
			return PrimaryBlock(*this) < other;
		}

		bool Bundle::operator>(const Bundle& other) const
		{
			return PrimaryBlock(*this) > other;
		}

		const std::list<const dtn::data::Block*> Bundle::getBlocks() const
		{
			return _blocks.getList();
		}

		void Bundle::remove(const dtn::data::Block &block)
		{
			_blocks.remove(&block);
		}

		void Bundle::clearBlocks()
		{
			_blocks.clear();
		}

		dtn::data::PayloadBlock& Bundle::insert(const dtn::data::Block &before, ibrcommon::BLOB::Reference &ref)
		{
			dtn::data::PayloadBlock *tmpblock = new dtn::data::PayloadBlock(ref);
			dtn::data::Block *block = dynamic_cast<dtn::data::Block*>(tmpblock);
			assert(block != NULL);
			_blocks.insert(block, &before);
			return (*tmpblock);
		}

		dtn::data::PayloadBlock& Bundle::push_front(ibrcommon::BLOB::Reference &ref)
		{
			dtn::data::PayloadBlock *tmpblock = new dtn::data::PayloadBlock(ref);
			dtn::data::Block *block = dynamic_cast<dtn::data::Block*>(tmpblock);
			assert(block != NULL);
			_blocks.push_front(block);
			return (*tmpblock);
		}

		dtn::data::PayloadBlock& Bundle::push_back(ibrcommon::BLOB::Reference &ref)
		{
			dtn::data::PayloadBlock *tmpblock = new dtn::data::PayloadBlock(ref);
			dtn::data::Block *block = dynamic_cast<dtn::data::Block*>(tmpblock);
			assert(block != NULL);
			_blocks.push_back(block);
			return (*tmpblock);
		}

		dtn::data::Block& Bundle::push_back(dtn::data::ExtensionBlockFactory &factory)
		{
			dtn::data::Block *block = factory.create();
			assert(block != NULL);
			_blocks.push_back(block);
			return (*block);
		}

		string Bundle::toString() const
		{
			return PrimaryBlock::toString();
		}
	}
}

