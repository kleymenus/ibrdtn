#ifndef SIMPLEBUNDLESTORAGE_H_
#define SIMPLEBUNDLESTORAGE_H_

#include "Component.h"
#include "core/BundleCore.h"
#include "core/BundleStorage.h"
#include "core/Node.h"
#include "core/EventReceiver.h"
#include "routing/BundleList.h"

#include <ibrcommon/thread/Conditional.h>
#include <ibrcommon/thread/AtomicCounter.h>
#include <ibrcommon/thread/Mutex.h>

#include <ibrcommon/data/File.h>
#include <ibrdtn/data/Bundle.h>

#include <set>

using namespace dtn::data;

namespace dtn
{
	namespace core
	{
		/**
		 * This storage holds all bundles and fragments in the system memory.
		 */
		class SimpleBundleStorage : public BundleStorage, public EventReceiver, public dtn::daemon::IntegratedComponent
		{
		public:
			/**
			 * Iterator for this BundleStorage implementation
			 */
			class Iterator : public std::iterator<std::forward_iterator_tag, dtn::data::BundleID>, public ibrcommon::Mutex
			{
			public:
				Iterator(std::set<dtn::data::Bundle>::iterator iter, ibrcommon::Mutex &mutex);
				virtual ~Iterator();

				/**
				 * Mutex methods to lock the iterator
				 */
				void enter();
				void leave();

				// The assignment and relational operators are straightforward
				Iterator& operator=(const Iterator& other);

				bool operator==(const Iterator& other);

				bool operator!=(const Iterator& other);

				// Get the next element.
				Iterator& operator++();

				Iterator& operator++(int);

				// Return a reference to the value in the node.  I do this instead
				// of returning by value so a caller can update the value in the
				// node directly.
				const dtn::data::Bundle& operator*();

				// Return the address of the value referred to.
				const dtn::data::Bundle* operator->();

			private:
				ibrcommon::Mutex &_mutex;
				std::set<dtn::data::Bundle>::iterator _iter;
			};

			/**
			 * Iterator methods
			 */
			Iterator begin();
			Iterator end();

			/**
			 * Constructor
			 */
			SimpleBundleStorage();

			/**
			 * Constructor
			 */
			SimpleBundleStorage(const ibrcommon::File &workdir);

			/**
			 * Destructor
			 */
			virtual ~SimpleBundleStorage();

			/**
			 * Stores a bundle in the storage.
			 * @param bundle The bundle to store.
			 */
			virtual void store(const dtn::data::Bundle &bundle);

			/**
			 * This method returns a specific bundle which is identified by
			 * its id.
			 * @param id The ID of the bundle to return.
			 * @return A bundle object of the
			 */
			virtual dtn::data::Bundle get(const dtn::data::BundleID &id);

			virtual dtn::data::Bundle get(const dtn::data::EID &eid);

			/**
			 * This method deletes a specific bundle in the storage.
			 * No reports will be generated here.
			 * @param id The ID of the bundle to remove.
			 */
			void remove(const dtn::data::BundleID &id);

			/**
			 * @sa BundleStorage::clear()
			 */
			void clear();

			/**
			 * @sa BundleStorage::empty()
			 */
			bool empty();

			/**
			 * @sa BundleStorage::count()
			 */
			unsigned int count();

			/**
			 * This method is used to receive events.
			 * @param evt
			 */
			void raiseEvent(const Event *evt);

		protected:
			virtual void componentUp();
			virtual void componentDown();

		private:
			enum RunMode
			{
				MODE_NONPERSISTENT = 0,
				MODE_PERSISTENT = 1
			};

			class BundleStore : private dtn::routing::BundleList
			{
			public:
				BundleStore(SimpleBundleStorage &sbs);
				~BundleStore();

				void store(const dtn::data::Bundle &bundle);
				void remove(const dtn::data::BundleID &id);
				void clear();

				void expire(const size_t timestamp);

				std::set<dtn::data::Bundle> bundles;
				ibrcommon::Mutex bundleslock;

			protected:
				virtual void eventBundleExpired(const ExpiringBundle &b);

			private:
				SimpleBundleStorage &_sbs;
			};

			BundleStore _store;

			bool _running;
			RunMode _mode;
			ibrcommon::File _workdir;
			std::map<dtn::data::BundleID, ibrcommon::File> _bundlefiles;

			ibrcommon::Conditional _dbchanged;
		};
	}
}

#endif /*SIMPLEBUNDLESTORAGE_H_*/