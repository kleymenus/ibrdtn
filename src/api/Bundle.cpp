/*
 * Bundle.cpp
 *
 *  Created on: 24.07.2009
 *      Author: morgenro
 */

#include "ibrdtn/api/Bundle.h"
#include "ibrdtn/api/SecurityManager.h"
#include "ibrdtn/data/PayloadBlock.h"
#include "ibrdtn/data/Exceptions.h"

namespace dtn
{
	namespace api
	{
		Bundle::Bundle()
		 : _priority(PRIO_MEDIUM)
		{
			_security = SecurityManager::getDefault();
		}

		Bundle::Bundle(dtn::data::Bundle &b)
		 : _b(b), _priority(PRIO_MEDIUM)
		{
			// read priority
			if (_b._procflags & dtn::data::Bundle::PRIORITY_BIT1)
			{
				if (_b._procflags & dtn::data::Bundle::PRIORITY_BIT2) _priority = PRIO_HIGH;
				else _priority = PRIO_MEDIUM;
			}
			else
			{
				_priority = PRIO_LOW;
			}
		}

		Bundle::Bundle(dtn::data::EID destination)
		 : _priority(PRIO_MEDIUM)
		{
			_b._destination = destination;
			_security = SecurityManager::getDefault();
		}

		Bundle::~Bundle()
		{
		}

		dtn::blob::BLOBReference Bundle::getData()
		{
			const list<dtn::data::Block* > blocks = _b.getBlocks(dtn::data::PayloadBlock::BLOCK_TYPE);
			if (blocks.size() < 1) throw dtn::exceptions::MissingObjectException("No payload block exists!");

			dtn::data::Block *block = (*blocks.begin());
			return block->getBLOBReference();
		}

		void Bundle::read(std::istream &stream)
		{
			stream >> _b;

			// read priority
			if (_b._procflags & dtn::data::Bundle::PRIORITY_BIT1)
			{
				if (_b._procflags & dtn::data::Bundle::PRIORITY_BIT2) _priority = PRIO_HIGH;
				else _priority = PRIO_MEDIUM;
			}
			else
			{
				_priority = PRIO_LOW;
			}
		}

		void Bundle::write(std::ostream &stream)
		{
			// set the priority to the real bundle
			switch (_priority)
			{
			case PRIO_LOW:
				if (_b._procflags & dtn::data::Bundle::PRIORITY_BIT1) _b._procflags -= dtn::data::Bundle::PRIORITY_BIT1;
				if (_b._procflags & dtn::data::Bundle::PRIORITY_BIT2) _b._procflags -= dtn::data::Bundle::PRIORITY_BIT2;
				break;

			case PRIO_HIGH:
				if (_b._procflags & dtn::data::Bundle::PRIORITY_BIT1) _b._procflags -= dtn::data::Bundle::PRIORITY_BIT1;
				if (!(_b._procflags & dtn::data::Bundle::PRIORITY_BIT2)) _b._procflags += dtn::data::Bundle::PRIORITY_BIT2;
				break;

			case PRIO_MEDIUM:
				if (!(_b._procflags & dtn::data::Bundle::PRIORITY_BIT1)) _b._procflags += dtn::data::Bundle::PRIORITY_BIT1;
				if (_b._procflags & dtn::data::Bundle::PRIORITY_BIT2) _b._procflags -= dtn::data::Bundle::PRIORITY_BIT2;
				break;
			}

			// send the bundle
			stream << _b;
		}
	}
}
