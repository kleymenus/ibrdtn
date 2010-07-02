#ifndef SIMPLEBUNDLESTORAGE_H_
#define SIMPLEBUNDLESTORAGE_H_

#include "Component.h"
#include "core/BundleCore.h"
#include "core/BundleStorage.h"
#include "core/Node.h"
#include "core/EventReceiver.h"

#include <ibrcommon/thread/Conditional.h>
#include <ibrcommon/thread/AtomicCounter.h>
#include <ibrcommon/thread/Mutex.h>

#include <ibrcommon/data/File.h>
#include <ibrdtn/data/Bundle.h>
#include <ibrdtn/data/BundleList.h>
#include <ibrcommon/thread/ThreadSafeQueue.h>

#include <set>

using namespace dtn::data;

namespace dtn
{
	namespace core
	{
		/**
		 * This storage holds all bundles and fragments in the system memory.
		 */
		class SimpleBundleStorage : public BundleStorage, public EventReceiver, public dtn::daemon::IndependentComponent
		{
			class Task
			{
			public:
				virtual ~Task() {};
				virtual void run() = 0;
			};

		public:
			/**
			 * Constructor
			 */
			SimpleBundleStorage(size_t maxsize = 0);

			/**
			 * Constructor
			 */
			SimpleBundleStorage(const ibrcommon::File &workdir, size_t maxsize = 0);

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


			const std::list<dtn::data::BundleID> getList();

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
			 * Get the current size
			 */
			size_t size() const;

			/**
			 * @sa BundleStorage::releaseCustody();
			 */
			void releaseCustody(dtn::data::BundleID &bundle);

			/**
			 * This method is used to receive events.
			 * @param evt
			 */
			void raiseEvent(const Event *evt);

		protected:
			virtual void componentUp();
			virtual void componentRun();
			virtual void componentDown();

		private:
			enum RunMode
			{
				MODE_NONPERSISTENT = 0,
				MODE_PERSISTENT = 1
			};

			class BundleContainer : public dtn::data::BundleID
			{
			public:
				BundleContainer(const dtn::data::Bundle &b);
				BundleContainer(const ibrcommon::File &file);
				BundleContainer(const dtn::data::Bundle &b, const ibrcommon::File &workdir, const size_t size);
				~BundleContainer();

				bool operator!=(const BundleContainer& other) const;
				bool operator==(const BundleContainer& other) const;
				bool operator<(const BundleContainer& other) const;
				bool operator>(const BundleContainer& other) const;

				size_t size() const;

				dtn::data::Bundle& operator*();
				const dtn::data::Bundle& operator*() const;

				BundleContainer& operator= (const BundleContainer &right);
				BundleContainer& operator= (BundleContainer &right);
				BundleContainer(const BundleContainer& right);

				void invokeStore();

				void remove();

			protected:
				class Holder
				{
				public:
					Holder( const dtn::data::Bundle &b );
					Holder( const ibrcommon::File &file );
					Holder( const dtn::data::Bundle &b, const ibrcommon::File &workdir, const size_t size );
					~Holder();

					size_t size() const;

					void invokeStore();

					dtn::data::Bundle _bundle;
					ibrcommon::File _container;
					RunMode _mode;
					unsigned _count;

					bool deletion;

					size_t _size;
				};

			private:
				Holder *_holder;
			};

			class TaskStoreBundle : public Task
			{
			public:
				TaskStoreBundle(const SimpleBundleStorage::BundleContainer&);
				~TaskStoreBundle();
				virtual void run();

			private:
				SimpleBundleStorage::BundleContainer _container;
			};

			class BundleStore : private dtn::data::BundleList
			{
			public:
				BundleStore(size_t maxsize = 0);
				BundleStore(ibrcommon::File workdir, size_t maxsize = 0);
				~BundleStore();

				void load(const ibrcommon::File &file);
				void store(const dtn::data::Bundle &bundle, SimpleBundleStorage &storage);
				void remove(const dtn::data::BundleID &id);
				void clear();

				unsigned int count();
				size_t size() const;

				dtn::data::Bundle get(const dtn::data::EID &eid);
				dtn::data::Bundle get(const dtn::data::BundleID &id);

				const std::list<dtn::data::BundleID> getList();

				void expire(const size_t timestamp);

				std::set<BundleContainer> bundles;
				ibrcommon::Mutex bundleslock;

			protected:
				virtual void eventBundleExpired(const ExpiringBundle &b);

			private:
				ibrcommon::File _workdir;
				RunMode _mode;

				size_t _maxsize;
				size_t _currentsize;
			};

			BundleStore _store;
			bool _running;

			ibrcommon::ThreadSafeQueue<Task*> _tasks;
		};
	}
}

#endif /*SIMPLEBUNDLESTORAGE_H_*/
