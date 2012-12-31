#ifndef __SBC_EVENTS_H
#define __SBC_EVENTS_H

#include "CallLeg.h"

enum {
  ChangeRtpModeEventId = LAST_B2B_CALL_LEG_EVENT_ID + 1,

  /** This ID should be used by all CC modules that produce B2B events. Because
   * CC modules are developped indepenedently it is not possible to synchronize
   * their B2B event IDs so only this one should be used. */
  CCB2BEventId
};

/* we don't need to have 'reliable event' for this because we are always
 * connected to SBCCallLeg, right? */
struct ChangeRtpModeEvent: public B2BEvent
{
  AmB2BSession::RTPRelayMode new_mode;
  AmB2BMedia *media; // avoid direct access to this

  ChangeRtpModeEvent(AmB2BSession::RTPRelayMode _new_mode, AmB2BMedia *_media):
    B2BEvent(ChangeRtpModeEventId), new_mode(_new_mode), media(_media)
    { if (media) media->addReference(); }

  virtual ~ChangeRtpModeEvent() { if (media && media->releaseReference()) delete media; }
};


#endif
