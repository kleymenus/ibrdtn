/*
 * SummaryVector.cpp
 *
 *  Created on: 02.03.2010
 *      Author: morgenro
 */

#include "routing/SummaryVector.h"

namespace dtn
{
	namespace routing
	{
		SummaryVector::SummaryVector(const std::set<dtn::routing::MetaBundle> &list)
		 : _bf(4096, 4)
		{
			add(list);
		}

		SummaryVector::SummaryVector()
		 : _bf(4096, 4)
		{
		}

		SummaryVector::~SummaryVector()
		{
		}

		void SummaryVector::add(const std::set<dtn::routing::MetaBundle> &list)
		{
			for (std::set<dtn::routing::MetaBundle>::const_iterator iter = list.begin(); iter != list.end(); iter++)
			{
				_bf.insert( (*iter).toString() );
				_ids.push_back( (*iter) );
			}
		}

		bool SummaryVector::contains(const dtn::data::BundleID &id) const
		{
			return _bf.contains(id.toString());
		}

		void SummaryVector::add(const dtn::data::BundleID &id)
		{
			_bf.insert(id.toString());
			_ids.push_back( id );
		}

		void SummaryVector::clear()
		{
			_bf.clear();
			_ids.clear();
		}

		const ibrcommon::BloomFilter& SummaryVector::getBloomFilter() const
		{
			return _bf;
		}

		const std::list<dtn::data::BundleID> SummaryVector::getNotIn(ibrcommon::BloomFilter &filter) const
		{
			std::list<dtn::data::BundleID> ret;

//			// if the lists are equal return an empty list
//			if (filter == _bf) return ret;

			// iterate through all items to find the differences
			for (std::list<dtn::data::BundleID>::const_iterator iter = _ids.begin(); iter != _ids.end(); iter++)
			{
				if (_bf.contains( (*iter).toString() ) )
				{
					ret.push_back( (*iter) );
				}
			}

			return ret;
		}

		std::ostream &operator<<(std::ostream &stream, const SummaryVector &obj)
		{
			dtn::data::SDNV size(obj._bf.size());
			stream << size;

			const char *data = reinterpret_cast<const char*>(obj._bf.table());
			stream.write(data, size.getValue());
		}

		std::istream &operator>>(std::istream &stream, SummaryVector &obj)
		{
			dtn::data::SDNV count;
			stream >> count;

			char buffer[count.getValue()];

			stream.read(buffer, count.getValue());

			obj.clear();
			obj._bf.load((unsigned char*)buffer, count.getValue());
		}
	}
}