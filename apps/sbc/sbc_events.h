#ifndef __SBC_EVENTS_H
#define __SBC_EVENTS_H

#include "CallLeg.h"

enum {
  /** This ID should be used by all CC modules that produce B2B events. Because
   * CC modules are developped indepenedently it is not possible to synchronize
   * their B2B event IDs so only this one should be used. */
  CCB2BEventId = LAST_B2B_CALL_LEG_EVENT_ID + 1
};

#endif
