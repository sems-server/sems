#include "RateLimit.h"
#include "AmAppTimer.h"

#define min(a,b) ((a) < (b) ? (a) : (b))

DynRateLimit::DynRateLimit(unsigned int time_base_ms)
  : last_update(0), counter(0)
{
  // wall_clock has a resolution of 20ms
  time_base = time_base_ms / 20;
}

bool DynRateLimit::limit(unsigned int rate, unsigned int peak, 
			 unsigned int size)
{
  lock();

  if(AmAppTimer::instance()->wall_clock - last_update 
     > time_base) {

    update_limit(rate,peak);
  }

  if(counter <= 0) {
    unlock();
    return true; // limit reached
  }

  counter -= size;
  unlock();

  return false; // do not limit
}

void DynRateLimit::update_limit(int rate, int peak)
{
  counter = min(peak, counter+rate);
  last_update = AmAppTimer::instance()->wall_clock;
}
