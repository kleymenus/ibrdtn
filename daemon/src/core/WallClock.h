/*
 * Clock.h
 *
 *  Created on: 17.07.2009
 *      Author: morgenro
 */

#ifndef WALLCLOCK_H_
#define WALLCLOCK_H_

#include <ibrcommon/thread/Thread.h>
#include <ibrcommon/thread/Conditional.h>
#include <ibrcommon/thread/Timer.h>
#include "Component.h"

namespace dtn
{
	namespace core
	{
		class WallClock : public ibrcommon::Conditional, public dtn::daemon::IntegratedComponent, public ibrcommon::SimpleTimerCallback
		{
		public:
			/**
			 * Constructor for the global Clock
			 * @param frequency Specify the frequency for the clock tick in seconds.
			 */
			WallClock(size_t frequency);
			virtual ~WallClock();

			/**
			 * Blocks until the next clock tick happens.
			 */
			void sync();

			/**
			 * timer callback method
			 * @see SimpleTimerCallback::timeout()
			 */
			virtual size_t timeout(size_t identifier);

		protected:
			virtual void componentUp();
			virtual void componentDown();

		private:
			size_t _frequency;
			size_t _next;
			ibrcommon::SimpleTimer _timer;

		};
	}
}

#endif /* WALLCLOCK_H_ */
