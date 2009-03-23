#ifndef SIMPLEBUNDLESTORAGE_H_
#define SIMPLEBUNDLESTORAGE_H_

#include "core/BundleCore.h"
#include "core/BundleStorage.h"
#include "core/EventReceiver.h"
#include "core/Node.h"
#include "data/Bundle.h"
#include "utils/Service.h"
#include "utils/MutexLock.h"
#include "utils/Mutex.h"
#include "utils/Conditional.h"
#include <list>
#include <map>

using namespace dtn::data;
using namespace dtn::utils;

namespace dtn
{
	namespace core
	{
		/**
		 * This storage holds all bundles, fragments and schedules in the system memory.
		 */
		class SimpleBundleStorage : public Service, public BundleStorage, public EventReceiver
		{
		public:
			/**
			 * Constructor
			 * @param[in] size Size of the memory storage in kBytes. (e.g. 1024*1024 = 1MB)
			 * @param[in] bundle_maxsize Maximum size of one bundle in bytes.
			 * @param[in] merge Do database cleanup and merging.
			 */
			SimpleBundleStorage(unsigned int size = 1024 * 1024, unsigned int bundle_maxsize = 1024, bool merge = false);

			/**
			 * Destructor
			 */
			virtual ~SimpleBundleStorage();

			/**
			 * @sa BundleStorage::clear()
			 */
			void clear();

			/**
			 * @sa BundleStorage::isEmpty()
			 */
			bool isEmpty();

			/**
			 * @sa BundleStorage::getCount()
			 */
			unsigned int getCount();

			/**
			 * @sa Service::tick()
			 */
			virtual void tick();

			/**
			 * receive event from the event switch
			 */
			void raiseEvent(const Event *evt);

		protected:
			void terminate();

		private:
			/**
			 * @sa BundleStorage::store(BundleSchedule schedule)
			 */
			void store(const BundleSchedule &schedule);

			/**
			 * @sa getSchedule(unsigned int dtntime)
			 */
			BundleSchedule getSchedule(unsigned int dtntime);

			/**
			 * @sa getSchedule(string destination)
			 */
			BundleSchedule getSchedule(string destination);

			/**
			 * @sa storeFragment();
			 */
			Bundle* storeFragment(const Bundle &bundle);

			list<BundleSchedule> m_schedules;
			list<list<Bundle> > m_fragments;

			/**
			 * Try to find outdated bundles and delete them.
			 * Additional a deleted report is created if necessary.
			 */
			void deleteDeprecated();

			/**
			 * A variable to hold the next timeout of a bundle in the storage.
			 */
			unsigned int m_nextdeprecation;

			time_t m_last_compress;

			unsigned int m_size;
			unsigned int m_bundle_maxsize;

			unsigned int m_currentsize;

			bool m_nocleanup;

			Mutex m_readlock;
			Mutex m_fragmentslock;
			Conditional m_breakwait;

			bool m_merge;

			map<string,Node> m_neighbours;
		};
	}
}

#endif /*SIMPLEBUNDLESTORAGE_H_*/
