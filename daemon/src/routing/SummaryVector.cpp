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
		SummaryVector::SummaryVector(const std::set<dtn::data::MetaBundle> &list)
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

		void SummaryVector::rebuild()
		{
			for (std::set<dtn::data::BundleID>::const_iterator iter = _ids.begin(); iter != _ids.end(); iter++)
			{
				_bf.insert( (*iter).toString() );
			}
		}

		void SummaryVector::add(const std::set<dtn::data::MetaBundle> &list)
		{
			for (std::set<dtn::data::MetaBundle>::const_iterator iter = list.begin(); iter != list.end(); iter++)
			{
				_bf.insert( (*iter).toString() );
				_ids.insert( (*iter) );
			}
		}

		bool SummaryVector::contains(const dtn::data::BundleID &id) const
		{
			return _bf.contains(id.toString());
		}

		void SummaryVector::add(const dtn::data::BundleID &id)
		{
			_bf.insert(id.toString());
			_ids.insert( id );
		}

		void SummaryVector::remove(const dtn::data::BundleID &id)
		{
			_ids.erase( id );
			rebuild();
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

		std::set<dtn::data::BundleID> SummaryVector::getNotIn(ibrcommon::BloomFilter &filter) const
		{
			std::set<dtn::data::BundleID> ret;

//			// if the lists are equal return an empty list
//			if (filter == _bf) return ret;

			// iterate through all items to find the differences
			for (std::set<dtn::data::BundleID>::const_iterator iter = _ids.begin(); iter != _ids.end(); iter++)
			{
				if (!filter.contains( (*iter).toString() ) )
				{
					ret.insert( (*iter) );
				}
			}

			return ret;
		}

		size_t SummaryVector::getLength() const
		{
			return dtn::data::SDNV(_bf.size()).getLength() + _bf.size();
		}

		std::ostream &operator<<(std::ostream &stream, const SummaryVector &obj)
		{
			dtn::data::SDNV size(obj._bf.size());
			stream << size;

			const char *data = reinterpret_cast<const char*>(obj._bf.table());
			stream.write(data, obj._bf.size());

			return stream;
		}

		std::istream &operator>>(std::istream &stream, SummaryVector &obj)
		{
			dtn::data::SDNV count;
			stream >> count;

			char buffer[count.getValue()];

			stream.read(buffer, count.getValue());

			obj.clear();
			obj._bf.load((unsigned char*)buffer, count.getValue());

			return stream;
		}
	}
}
