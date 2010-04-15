/*
 * Graveyard.h
 *
 *  Created on: 28.10.2009
 *      Author: morgenro
 */

#ifndef GRAVEYARD_H_
#define GRAVEYARD_H_

#include <ibrcommon/thread/Thread.h>
#include <ibrcommon/thread/Conditional.h>
#include <queue>

namespace dtn
{
	namespace core
	{
		class Graveyard : public ibrcommon::JoinableThread
		{
		public:
			class Zombie
			{
			public:
				Zombie();
				virtual ~Zombie();
				virtual void embalm() = 0;
				void bury();

			private:
				bool _buried;
			};

			Graveyard();
			virtual ~Graveyard();

			static void bury(Zombie *z);

		protected:
			void run();

		private:
			bool wait();
			ibrcommon::Conditional _wait;

			void newJob(Zombie *z);

			bool _running;
			std::queue<Zombie*> _zombies;
		};
	}
}

#endif /* GRAVEYARD_H_ */
