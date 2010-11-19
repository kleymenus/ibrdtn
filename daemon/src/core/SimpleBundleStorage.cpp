#include "core/SimpleBundleStorage.h"
#include "core/TimeEvent.h"
#include "core/GlobalEvent.h"
#include "core/BundleExpiredEvent.h"
#include "core/BundleEvent.h"

#include <ibrdtn/data/AgeBlock.h>
#include <ibrdtn/utils/Utils.h>
#include <ibrcommon/thread/MutexLock.h>
#include <ibrcommon/Logger.h>
#include <ibrcommon/AutoDelete.h>

#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>

namespace dtn
{
	namespace core
	{
		SimpleBundleStorage::SimpleBundleStorage(size_t maxsize)
		 : _running(true), _mode(MODE_NONPERSISTENT), _maxsize(maxsize), _currentsize(0)
		{
		}

		SimpleBundleStorage::SimpleBundleStorage(const ibrcommon::File &workdir, size_t maxsize)
		 : _running(true), _workdir(workdir), _mode(MODE_PERSISTENT), _maxsize(maxsize), _currentsize(0)
		{
			// load persistent bundles
			std::list<ibrcommon::File> files;
			_workdir.getFiles(files);

			for (std::list<ibrcommon::File>::iterator iter = files.begin(); iter != files.end(); iter++)
			{
				ibrcommon::File &file = (*iter);
				if (!file.isDirectory() && !file.isSystem())
				{
					try {
						// load a bundle into the storage
						load(file);
					} catch (const ibrcommon::Exception &ex) {
						// report this error to the console
						IBRCOMMON_LOGGER(error) << "Error: Unable to restore bundle in file " << file.getPath() << IBRCOMMON_LOGGER_ENDL;

						// error while reading file
						file.remove();
					}
				}
			}

			// some output
			IBRCOMMON_LOGGER(info) << _bundles.size() << " Bundles restored." << IBRCOMMON_LOGGER_ENDL;
		}

		SimpleBundleStorage::~SimpleBundleStorage()
		{
			_tasks.abort();
			join();
		}

		void SimpleBundleStorage::componentUp()
		{
			bindEvent(TimeEvent::className);
		}

		void SimpleBundleStorage::componentDown()
		{
			unbindEvent(TimeEvent::className);

			_tasks.abort();
		}

		void SimpleBundleStorage::componentRun()
		{
			try {
				while (true)
				{
					Task *t = _tasks.getnpop(true);
					ibrcommon::AutoDelete<Task> ad(t);

					try {
						t->run(*this);
					} catch (const std::exception &ex) {
						IBRCOMMON_LOGGER(error) << "error in storage: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
					}

					yield();
				}
			} catch (const std::exception &ex) {
				IBRCOMMON_LOGGER_DEBUG(10) << "SimpleBundleStorage exited with " << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		bool SimpleBundleStorage::__cancellation()
		{
			_tasks.abort();
			return true;
		}

		void SimpleBundleStorage::raiseEvent(const Event *evt)
		{
			try {
				const TimeEvent &time = dynamic_cast<const TimeEvent&>(*evt);
				if (time.getAction() == dtn::core::TIME_SECOND_TICK)
				{
					_tasks.push( new SimpleBundleStorage::TaskExpireBundles(time.getTimestamp()) );
				}
			} catch (const std::bad_cast&) { }
		}

		const std::string SimpleBundleStorage::getName() const
		{
			return "SimpleBundleStorage";
		}

		bool SimpleBundleStorage::empty()
		{
			ibrcommon::MutexLock l(_bundleslock);
			return _bundles.empty();
		}

		void SimpleBundleStorage::releaseCustody(dtn::data::BundleID&)
		{
			// custody is successful transferred to another node.
			// it is safe to delete this bundle now. (depending on the routing algorithm.)
		}

		unsigned int SimpleBundleStorage::count()
		{
			ibrcommon::MutexLock l(_bundleslock);
			return _bundles.size();
		}

		const dtn::data::MetaBundle SimpleBundleStorage::getByFilter(const ibrcommon::BloomFilter &filter)
		{
			// search for one bundle which is not contained in the filter
			// until we have a better strategy, we have to iterate through all bundles
			ibrcommon::MutexLock l(_bundleslock);
			for (std::set<BundleContainer>::const_iterator iter = _bundles.begin(); iter != _bundles.end(); iter++)
			{
				const BundleContainer &container = (*iter);

				try {
					if ( !filter.contains(container.toString()) )
					{
						// check if the destination is blocked by the destination filter
						if ( (_destination_filter.find(container.destination) == _destination_filter.end()) )
						{
							IBRCOMMON_LOGGER_DEBUG(10) << container.toString() << " is not in the bloomfilter" << IBRCOMMON_LOGGER_ENDL;
							return container;
						}
					}
				} catch (const dtn::InvalidDataException &ex) {
					IBRCOMMON_LOGGER_DEBUG(10) << "InvalidDataException on bundle get: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
				}
			}

			throw BundleStorage::NoBundleFoundException();
		}

		const dtn::data::MetaBundle SimpleBundleStorage::getByDestination(const dtn::data::EID &eid, bool exact)
		{
			IBRCOMMON_LOGGER_DEBUG(5) << "Storage: get bundle for " << eid.getString() << IBRCOMMON_LOGGER_ENDL;
			
			ibrcommon::MutexLock l(_bundleslock);
			for (std::set<BundleContainer>::const_iterator iter = _bundles.begin(); iter != _bundles.end(); iter++)
			{
				const BundleContainer &bundle = (*iter);

				try {
					if (exact)
					{
						if (bundle.destination == eid)
						{
							return bundle;
						}
					}
					else
					{
						if (bundle.destination.getNodeEID() == eid.getNodeEID())
						{
							return bundle;
						}
					}
				} catch (const dtn::InvalidDataException &ex) {
					IBRCOMMON_LOGGER_DEBUG(10) << "InvalidDataException on bundle get: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
				}
			}

			throw BundleStorage::NoBundleFoundException();
		}

		dtn::data::Bundle SimpleBundleStorage::get(const dtn::data::BundleID &id)
		{
			try {
				ibrcommon::MutexLock l(_bundleslock);

				for (std::set<BundleContainer>::const_iterator iter = _bundles.begin(); iter != _bundles.end(); iter++)
				{
					const BundleContainer &bundle = (*iter);
					if (id == bundle)
					{
						return bundle.get();
					}
				}
			} catch (const dtn::SerializationFailedException &ex) {
				// bundle loading failed
				IBRCOMMON_LOGGER(error) << "Error while loading bundle data: " << ex.what() << IBRCOMMON_LOGGER_ENDL;

				// the bundle is broken, delete it
				remove(id);

				throw BundleStorage::BundleLoadException();
			}

			throw BundleStorage::NoBundleFoundException();
		}

		void SimpleBundleStorage::invokeExpiration(const size_t timestamp)
		{
			ibrcommon::MutexLock l(_bundleslock);
			dtn::data::BundleList::expire(timestamp);
		}

		void SimpleBundleStorage::load(const ibrcommon::File &file)
		{
			ibrcommon::MutexLock l(_bundleslock);

			BundleContainer container(file);
			_bundles.insert( container );

			// increment the storage size
			_currentsize += container.size();

			// add it to the bundle list
			dtn::data::BundleList::add(container);
		}

		void SimpleBundleStorage::store(const dtn::data::Bundle &bundle)
		{
			ibrcommon::MutexLock l(_bundleslock);

			// create a bundle container in the memory
			BundleContainer container(bundle);

			// check if this container is too big for us.
			if ((_maxsize > 0) && (_currentsize + container.size() > _maxsize))
			{
				throw StorageSizeExeededException();
			}

			// increment the storage size
			_currentsize += container.size();

			// if we are persistent, make a persistent container
			if (_mode == MODE_PERSISTENT)
			{
				container = BundleContainer(bundle, _workdir, container.size());

				// create a background task for storing the bundle
				_tasks.push( new SimpleBundleStorage::TaskStoreBundle(container) );
			}

			// insert Container
			pair<set<BundleContainer>::iterator,bool> ret = _bundles.insert( container );

			if (ret.second)
			{
				dtn::data::BundleList::add(dtn::data::MetaBundle(bundle));
			}
			else
			{
				IBRCOMMON_LOGGER_DEBUG(5) << "Storage: got bundle duplicate " << bundle.toString() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void SimpleBundleStorage::remove(const dtn::data::BundleID &id)
		{
			ibrcommon::MutexLock l(_bundleslock);

			for (std::set<BundleContainer>::const_iterator iter = _bundles.begin(); iter != _bundles.end(); iter++)
			{
				if ( id == (*iter) )
				{
					// remove item in the bundlelist
					BundleContainer container = (*iter);

					// remove it from the bundle list
					dtn::data::BundleList::remove(container);

					// decrement the storage size
					_currentsize -= container.size();

					// create a background task for removing the bundle
					_tasks.push( new SimpleBundleStorage::TaskRemoveBundle(container) );

					// remove the container
					_bundles.erase(iter);

					return;
				}
			}

			throw BundleStorage::NoBundleFoundException();
		}

		dtn::data::MetaBundle SimpleBundleStorage::remove(const ibrcommon::BloomFilter &filter)
		{
			ibrcommon::MutexLock l(_bundleslock);

			for (std::set<BundleContainer>::const_iterator iter = _bundles.begin(); iter != _bundles.end(); iter++)
			{
				const BundleContainer &container = (*iter);

				if ( filter.contains(container.toString()) )
				{
					// remove item in the bundlelist
					BundleContainer container = (*iter);

					// remove it from the bundle list
					dtn::data::BundleList::remove(container);

					// decrement the storage size
					_currentsize -= container.size();

					// create a background task for removing the bundle
					_tasks.push( new SimpleBundleStorage::TaskRemoveBundle(container) );

					// remove the container
					_bundles.erase(iter);

					return (MetaBundle)container;
				}
			}

			throw BundleStorage::NoBundleFoundException();
		}

		size_t SimpleBundleStorage::size() const
		{
			return _currentsize;
		}

		void SimpleBundleStorage::clear()
		{
			ibrcommon::MutexLock l(_bundleslock);

			// mark all bundles for deletion
			for (std::set<BundleContainer>::iterator iter = _bundles.begin(); iter != _bundles.end(); iter++)
			{
				BundleContainer container = (*iter);

				// mark for deletion
				container.remove();
			}

			_bundles.clear();
			dtn::data::BundleList::clear();

			// set the storage size to zero
			_currentsize = 0;
		}

		void SimpleBundleStorage::eventBundleExpired(const ExpiringBundle &b)
		{
			for (std::set<BundleContainer>::const_iterator iter = _bundles.begin(); iter != _bundles.end(); iter++)
			{
				if ( b.bundle == (*iter) )
				{
					BundleContainer container = (*iter);

					// decrement the storage size
					_currentsize -= container.size();

					container.remove();
					_bundles.erase(iter);
					break;
				}
			}

			// raise bundle event
			dtn::core::BundleEvent::raise( b.bundle, dtn::core::BUNDLE_DELETED, dtn::data::StatusReportBlock::LIFETIME_EXPIRED);

			// raise an event
			dtn::core::BundleExpiredEvent::raise( b.bundle );
		}

		const std::list<dtn::data::BundleID> SimpleBundleStorage::getList()
		{
			ibrcommon::MutexLock l(_bundleslock);
			std::list<dtn::data::BundleID> ret;

			for (std::set<BundleContainer>::const_iterator iter = _bundles.begin(); iter != _bundles.end(); iter++)
			{
				const BundleContainer &container = (*iter);
				ret.push_back( container );
			}

			return ret;
		}

		SimpleBundleStorage::BundleContainer::BundleContainer(const dtn::data::Bundle &b)
		 : dtn::data::MetaBundle(b), _holder( new SimpleBundleStorage::BundleContainer::Holder(b) )
		{
		}

		SimpleBundleStorage::BundleContainer::BundleContainer(const ibrcommon::File &file)
		 : dtn::data::MetaBundle(), _holder( new SimpleBundleStorage::BundleContainer::Holder(file) )
		{
			dtn::data::Bundle b = _holder->getBundle();
			(dtn::data::MetaBundle&)(*this) = dtn::data::MetaBundle(b);
		}

		SimpleBundleStorage::BundleContainer::BundleContainer(const dtn::data::Bundle &b, const ibrcommon::File &workdir, const size_t size)
		 : dtn::data::MetaBundle(b), _holder( new SimpleBundleStorage::BundleContainer::Holder(b, workdir, size) )
		{
		}

		SimpleBundleStorage::BundleContainer::~BundleContainer()
		{
			down();
		}

		void SimpleBundleStorage::BundleContainer::down()
		{
			try {
				ibrcommon::MutexLock l(_holder->lock);
				if (--_holder->_count == 0) throw true;
			} catch (bool) {
				delete _holder;
			}
		}

		size_t SimpleBundleStorage::BundleContainer::size() const
		{
			return _holder->size();
		}

		bool SimpleBundleStorage::BundleContainer::operator<(const SimpleBundleStorage::BundleContainer& other) const
		{
			MetaBundle &left = (MetaBundle&)*this;
			MetaBundle &right = (MetaBundle&)other;

			if (left.getPriority() > right.getPriority()) return true;
			if (left.getPriority() != right.getPriority()) return false;
			return (left < right);
		}

		SimpleBundleStorage::BundleContainer& SimpleBundleStorage::BundleContainer::operator= (const SimpleBundleStorage::BundleContainer &right)
		{
			ibrcommon::MutexLock l(right._holder->lock);
			++right._holder->_count;

			down();

			_holder = right._holder;
			return *this;
		}

		SimpleBundleStorage::BundleContainer::BundleContainer(const SimpleBundleStorage::BundleContainer& right)
		 : dtn::data::MetaBundle(right), _holder(right._holder)
		{
			ibrcommon::MutexLock l(_holder->lock);
			++_holder->_count;
		}

		dtn::data::Bundle SimpleBundleStorage::BundleContainer::get() const
		{
			return _holder->getBundle();
		}

		void SimpleBundleStorage::BundleContainer::remove()
		{
			_holder->remove();
		}

		void SimpleBundleStorage::BundleContainer::invokeStore()
		{
			_holder->invokeStore();
		}

		SimpleBundleStorage::BundleContainer::Holder::Holder( const dtn::data::Bundle &b )
		 :  _count(1), _state(HOLDER_MEMORY), _bundle(b)
		{
			dtn::data::DefaultSerializer s(std::cout);
			_size = s.getLength(_bundle);
		}

		SimpleBundleStorage::BundleContainer::Holder::Holder( const ibrcommon::File &file )
		 : _count(1), _state(HOLDER_STORED), _bundle(), _container(file)
		{
			_size = file.size();
		}

		SimpleBundleStorage::BundleContainer::Holder::Holder( const dtn::data::Bundle &b, const ibrcommon::File &workdir, const size_t size )
		 : _count(1), _state_lock(ibrcommon::Mutex::MUTEX_NORMAL), _state(HOLDER_PENDING), _bundle(b), _container(workdir), _size(size)
		{
		}

		SimpleBundleStorage::BundleContainer::Holder::~Holder()
		{
			ibrcommon::MutexLock l(_state_lock);
			if ((_state == HOLDER_DELETED) && (_container.exists()))
			{
				_container.remove();
			}
		}

		size_t SimpleBundleStorage::BundleContainer::Holder::size() const
		{
			return _size;
		}

		dtn::data::Bundle SimpleBundleStorage::BundleContainer::Holder::getBundle()
		{
			// the state must be protected with a mutex
			{
				ibrcommon::MutexLock l(_state_lock);
				if (_state == HOLDER_DELETED)
				{
					throw dtn::SerializationFailedException("bundle deleted");
				}
				else if (_state != HOLDER_STORED)
				{
					return _bundle;
				}
			}

			// we need to load the bundle from the storage
			dtn::data::Bundle bundle;

			// we need to load the bundle from file
			ibrcommon::locked_ifstream ostream(_container, ios::in|ios::binary);
			std::istream &fs = (*ostream);
			try {
				dtn::data::DefaultDeserializer(fs, dtn::core::BundleCore::getInstance()) >> bundle;
			} catch (const std::exception &ex) {
				throw dtn::SerializationFailedException("bundle get failed: " + std::string(ex.what()));
			}

			ostream.close();

			try {
				dtn::data::AgeBlock &agebl = bundle.getBlock<dtn::data::AgeBlock>();

				// modify the AgeBlock with the age of the file
				time_t age = _container.lastaccess() - _container.lastmodify();

				agebl.addAge(age);
			} catch (const dtn::data::Bundle::NoSuchBlockFoundException&) { };

			return bundle;
		}

		void SimpleBundleStorage::BundleContainer::Holder::remove()
		{
			ibrcommon::MutexLock l(_state_lock);
			_state = HOLDER_DELETED;
		}

		void SimpleBundleStorage::BundleContainer::Holder::invokeStore()
		{
			// check if this container need to be stored
			{
				ibrcommon::MutexLock l(_state_lock);
				if (_state != HOLDER_PENDING) return;
			}

			try {
				ibrcommon::TemporaryFile tmp(_container, "bundle");
				ibrcommon::locked_ofstream ostream(_container, ios_base::out | ios_base::trunc);
				std::ostream &out = (*ostream);
				size_t stream_size = 0;
				size_t bundle_size = 0;

				try {
					// check for failed stream
					if (ostream.fail()) throw ibrcommon::IOException("unable to open file " + tmp.getPath());

					dtn::data::DefaultSerializer s(out);
					bundle_size = s.getLength(_bundle);

					s << _bundle; out << std::flush; stream_size = out.tellp();

					if (stream_size == bundle_size)
					{
						std::stringstream errormsg; errormsg << "stream size (" << stream_size << ") do not match bundle size (" << bundle_size << ")";
						throw ibrcommon::IOException(errormsg.str());
					}
				} catch (const std::exception &ex) {
					// close the stream
					ostream.close();
					tmp.remove();
					throw;
				}

				// close the stream
				ostream.close();

				{
					ibrcommon::MutexLock l(_state_lock);
					_container = tmp;
					_size = tmp.size();
					_bundle = dtn::data::Bundle();

					if (_state == HOLDER_PENDING)
					{
						_state = HOLDER_STORED;
					}
				}
			} catch (const dtn::SerializationFailedException &ex) {
				throw dtn::SerializationFailedException("serialization failed: " + std::string(ex.what()));
			} catch (const std::exception &ex) {
				throw dtn::SerializationFailedException("can not write bundle: " + std::string(ex.what()));
			}
		}

		SimpleBundleStorage::TaskStoreBundle::TaskStoreBundle(const SimpleBundleStorage::BundleContainer &container, size_t retry)
		 : _container(container), _retry(retry)
		{ }

		SimpleBundleStorage::TaskStoreBundle::~TaskStoreBundle()
		{ }

		void SimpleBundleStorage::TaskStoreBundle::run(SimpleBundleStorage &storage)
		{
			try {
				_container.invokeStore();
				IBRCOMMON_LOGGER_DEBUG(20) << "bundle stored " << _container.toString() << " (size: " << _container.size() << ")" << IBRCOMMON_LOGGER_ENDL;
			} catch (const ibrcommon::Exception &ex) {
				// reschedule the task
				if (_retry < 10)
				{
					IBRCOMMON_LOGGER(warning) << "store failed " << _container.toString() << " (size: " << _container.size() << "): " << ex.what() << IBRCOMMON_LOGGER_ENDL;
					storage._tasks.push( new SimpleBundleStorage::TaskStoreBundle(_container, _retry + 1) );
				}
				else
				{
					IBRCOMMON_LOGGER(error) << "store failed " << _container.toString() << " (size: " << _container.size() << "): " << ex.what() << IBRCOMMON_LOGGER_ENDL;
				}
			}
		}

		SimpleBundleStorage::TaskRemoveBundle::TaskRemoveBundle(const SimpleBundleStorage::BundleContainer &container)
		 : _container(container)
		{ }

		SimpleBundleStorage::TaskRemoveBundle::~TaskRemoveBundle()
		{ }

		void SimpleBundleStorage::TaskRemoveBundle::run(SimpleBundleStorage&)
		{
			// mark for deletion
			_container.remove();
		}

		SimpleBundleStorage::TaskExpireBundles::TaskExpireBundles(const size_t &timestamp)
		 : _timestamp(timestamp)
		{ }

		SimpleBundleStorage::TaskExpireBundles::~TaskExpireBundles()
		{ }

		void SimpleBundleStorage::TaskExpireBundles::run(SimpleBundleStorage &storage)
		{
			storage.invokeExpiration(_timestamp);
		}
	}
}
