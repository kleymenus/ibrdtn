/*
 * TestSerializer.cpp
 *
 *  Created on: 30.07.2010
 *      Author: morgenro
 */

#include "data/TestSerializer.h"
#include <ibrdtn/data/EID.h>
#include <cppunit/extensions/HelperMacros.h>

#include <ibrdtn/data/Bundle.h>
#include <ibrdtn/data/Serializer.h>
#include <iostream>
#include <sstream>

CPPUNIT_TEST_SUITE_REGISTRATION (TestSerializer);

void TestSerializer::setUp(void)
{
}

void TestSerializer::tearDown(void)
{
}

void TestSerializer::serializer_separate01(void)
{
	dtn::data::Bundle b;
	ibrcommon::BLOB::Reference ref = ibrcommon::TmpFileBLOB::create();

	stringstream ss1;
	dtn::data::PayloadBlock &p1 = b.push_back(ref);
	p1.addEID(dtn::data::EID("dtn://test1234/app1234"));

	stringstream ss2;
	dtn::data::PayloadBlock &p2 = b.push_back(ref);
	p2.addEID(dtn::data::EID("dtn://test1234/app1234"));

	dtn::data::SeparateSerializer(ss1) << p1;
	dtn::data::SeparateSerializer(ss2) << p2;

	ss1.clear(); ss1.seekp(0); ss1.seekg(0);
	ss2.clear(); ss2.seekp(0); ss2.seekg(0);

	dtn::data::Bundle b2;

	dtn::data::SeparateDeserializer(ss1, b2).readBlock();
	dtn::data::SeparateDeserializer(ss2, b2).readBlock();

	assert( b2.getBlocks().size() == b.getBlocks().size() );
}

void TestSerializer::serializer_cbhe01(void)
{
	dtn::data::Bundle b;

	dtn::data::EID src("ipn:1.2");
	dtn::data::EID dst("ipn:2.3");
	dtn::data::EID report("ipn:6.1");

	std::pair<size_t, size_t> cbhe_eid = src.getCompressed();

	b._source = src;
	b._destination = dst;
	b._reportto = report;

	ibrcommon::BLOB::Reference ref = ibrcommon::TmpFileBLOB::create();
	b.push_back(ref);

	std::stringstream ss;
	dtn::data::DefaultSerializer(ss) << b;

	dtn::data::Bundle b2;
	ss.clear(); ss.seekp(0); ss.seekg(0);
	dtn::data::DefaultDeserializer(ss) >> b2;

	assert( b._source == b2._source );
	assert( b._destination == b2._destination );
	assert( b._reportto == b2._reportto );
	assert( b._custodian == b2._custodian );
}

void TestSerializer::serializer_cbhe02(void)
{
	dtn::data::Bundle b;

	dtn::data::EID src("ipn:1.2");
	dtn::data::EID dst("ipn:2.3");

	b._source = src;
	b._destination = dst;

	ibrcommon::BLOB::Reference ref = ibrcommon::TmpFileBLOB::create();
	(*ref) << "0123456789";
	b.push_back(ref);

	std::stringstream ss;
	dtn::data::DefaultSerializer(ss) << b;

	assert( ss.str().length() == 33);
}