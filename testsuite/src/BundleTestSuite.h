#ifndef BUNDLETESTSUITE_H_
#define BUNDLETESTSUITE_H_

#include "ibrdtn/data/Bundle.h"

using namespace dtn::data;

namespace dtn
{
namespace testsuite
{
	class BundleTestSuite
	{
		public:
			BundleTestSuite();

			virtual ~BundleTestSuite();

			bool runAllTests();

		private:
			bool serializeTest();

//			bool createTest();
//			bool copyTest();
//			bool instanceTest();
//			bool compareTest();
//			bool splitTest();
//			bool cutTest();
//			bool sliceTest();
//			bool mergeTest();
//			bool mergeBlockTest();
	};
}
}

#endif /*BUNDLETESTSUITE_H_*/
