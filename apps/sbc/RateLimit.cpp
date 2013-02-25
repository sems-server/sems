#include "RateLimit.h"
#include "AmAppTimer.h"

#define min(a,b) ((a) < (b) ? (a) : (b))

RateLimit::RateLimit(unsigned int rate, unsigned int peak, 
		     unsigned int time_base_seconds)
  : rate(rate),
    peak(peak),
    counter(peak)
{
  // wall_clock has a resolution of 20ms
  time_base = (1000 * time_base_seconds) / 20;
  last_update = AmAppTimer::instance()->wall_clock;
}

bool RateLimit::limit(unsigned int size)
{
  lock();

  if(AmAppTimer::instance()->wall_clock - last_update 
     > time_base) {

    update_limit();
  }

  if(counter <= 0) {
    unlock();
    return true; // limit reached
  }

  counter -= size;
  unlock();

  return false; // do not limit
}

void RateLimit::update_limit()
{
  counter = min(peak, counter+rate);
  last_update = AmAppTimer::instance()->wall_clock;
}
