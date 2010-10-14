/*
 * MetaBundle.cpp
 *
 *  Created on: 16.02.2010
 *      Author: morgenro
 */

#ifndef METABUNDLE_CPP_
#define METABUNDLE_CPP_

#include "ibrdtn/data/MetaBundle.h"

namespace dtn
{
	namespace data
	{
		MetaBundle::MetaBundle(const dtn::data::BundleID id, const dtn::data::DTNTime recv,
				const size_t l, const dtn::data::EID d, const dtn::data::EID r,
				const dtn::data::EID c, const size_t app, const size_t proc)
		 : BundleID(id), received(recv), lifetime(l), destination(d), reportto(r),
		   custodian(c), appdatalength(app), procflags(proc)
		{
		}

		MetaBundle::MetaBundle(const dtn::data::Bundle &b)
		 : BundleID(b), lifetime(b._lifetime), destination(b._destination), reportto(b._reportto),
		   custodian(b._custodian), appdatalength(b._appdatalength), procflags(b._procflags)
		{
		}

		MetaBundle::~MetaBundle()
		{}

		int MetaBundle::getPriority() const
		{
			// read priority
			if (procflags & dtn::data::Bundle::PRIORITY_BIT1)
			{
				return 0;
			}
			else
			{
				if (procflags & dtn::data::Bundle::PRIORITY_BIT2) return 1;
				else return -1;
			}
		}
	}
}

#endif /* METABUNDLE_CPP_ */
