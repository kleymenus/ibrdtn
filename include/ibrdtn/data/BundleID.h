/*
 * BundleID.h
 *
 *  Created on: 01.09.2009
 *      Author: morgenro
 */

#ifndef BUNDLEID_H_
#define BUNDLEID_H_

#include "ibrdtn/config.h"
#include "ibrdtn/data/EID.h"
#include "ibrdtn/data/Bundle.h"

namespace dtn
{
	namespace data
	{
		class BundleID
		{
		public:
			BundleID(const dtn::data::Bundle &b);
			BundleID(EID source, size_t timestamp, size_t sequencenumber);
			virtual ~BundleID();

			bool operator==(const BundleID& other) const;
			bool operator<(const BundleID& other) const;
			bool operator>(const BundleID& other) const;

			string toString() const;

		private:
			dtn::data::EID _source;
			size_t _timestamp;
			size_t _sequencenumber;
		};
	}
}

#endif /* BUNDLEID_H_ */