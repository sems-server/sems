#ifndef _RateLimit_h_
#define _RateLimit_h_

#include "AmThread.h"
#include "atomic_types.h"
#include <sys/types.h>

class DynRateLimit
  : protected AmMutex
{
  u_int32_t last_update;
  int counter;

  unsigned int time_base;

  void update_limit(int rate, int peak);

public:
  // time_base_ms: milliseconds
  DynRateLimit(unsigned int time_base_ms);

  virtual ~DynRateLimit() {}

  unsigned int getTimeBase() const { return time_base; }

  /**
   * rate: units/time_base
   * peak: units/time_base
   * returns true if 'size' should be dropped
   */
  bool limit(unsigned int rate, unsigned int peak, unsigned int size);

  /** Get last update timestamp (wheeltimer::wallclock ticks) */
  u_int32_t getLastUpdate() { return last_update; }
};

class RateLimit
  : protected DynRateLimit
{
  int rate;
  int peak;

public:
  // time_base_ms: milliseconds
  RateLimit(unsigned int rate, unsigned int peak, unsigned int time_base_ms)
    : DynRateLimit(time_base_ms), rate(rate), peak(peak)
  {}

  int getRate() const { return rate; }
  int getPeak() const { return peak; }
  unsigned int getTimeBase() const { return DynRateLimit::getTimeBase(); }

  /**
   * returns true if 'size' should be dropped
   */
  bool limit(unsigned int size) {
    return DynRateLimit::limit(rate,peak,size);
  }
};

#endif
