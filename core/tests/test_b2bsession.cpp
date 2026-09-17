#include "fct.h"

#include "log.h"

#include "AmB2BMedia.h"
#include "AmB2BSession.h"

// ~AmB2BSession() has to release the media session a leg still holds, in every
// RTP relay mode. AmB2BMedia keeps raw pointers to its legs, and the media
// processor keeps calling into them (processDtmfEvents()) for as long as the
// media session lives. clearRtpReceiverRelay() only releases it in RTP_Relay
// and RTP_Transcoding mode, while SBCDSMInstance::connectMedia() attaches a
// media session to a leg in RTP_Direct mode.
//
// Each test holds a reference of its own to the AmB2BMedia: releaseReference()
// returning true for it shows that the legs have released theirs. Legs in
// RTP_Direct mode log an ERROR when they are destroyed; that is expected here.

namespace {

class TestLeg : public AmB2BSession {
public:
  // calls from AmB2BMedia::processDtmfEvents()
  int dtmf_rounds;

  explicit TestLeg(bool is_a_leg) : dtmf_rounds(0) { a_leg = is_a_leg; }

  void processDtmfEvents() override { dtmf_rounds++; }

  // CallLeg::onB2BReconnect() does this: the role changes while the leg stays
  // registered in the AmB2BMedia slot it was created in
  void flipRole() { a_leg = !a_leg; }
};

const AmB2BSession::RTPRelayMode modes[] = {AmB2BSession::RTP_Direct, AmB2BSession::RTP_Relay,
                                            AmB2BSession::RTP_Transcoding};
const int nb_modes = sizeof(modes) / sizeof(modes[0]);

// set by the first test: without it, the second one would reach legs that
// have already been freed
bool destroyed_legs_release_media = false;

} // namespace

FCTMF_SUITE_BGN(test_b2bsession) {

  FCT_TEST_BGN(destroyed_leg_releases_its_media_session) {
    bool all_released = true;
    for (int m = 0; m < nb_modes; m++) {
      for (int side = 0; side < 2; side++) {
        bool is_a_leg = side == 0;
        TestLeg *leg = new TestLeg(is_a_leg);
        leg->setRtpRelayMode(modes[m]);
        AmB2BMedia *media = new AmB2BMedia(is_a_leg ? leg : NULL, is_a_leg ? NULL : leg);
        media->addReference();
        leg->setMediaSession(media);

        delete leg;
        // if the leg kept its reference, the media session is leaked here
        bool released = media->releaseReference();
        fct_xchk(released, "relay mode %d, %s leg: media session reference not released", (int)modes[m],
                 is_a_leg ? "A" : "B");
        all_released = all_released && released;
      }
    }
    destroyed_legs_release_media = all_released;
  }
  FCT_TEST_END();

  FCT_TEST_BGN(destroyed_leg_is_detached_from_a_shared_media_session) {
    fct_req(destroyed_legs_release_media);
    for (int m = 0; m < nb_modes; m++) {
      for (int side = 0; side < 2; side++) {
        bool leg_is_a = side == 0;
        TestLeg *leg = new TestLeg(leg_is_a);
        TestLeg *peer = new TestLeg(!leg_is_a);
        leg->setRtpRelayMode(modes[m]);
        peer->setRtpRelayMode(modes[m]);
        AmB2BMedia *media = new AmB2BMedia(leg_is_a ? leg : peer, leg_is_a ? peer : leg);
        media->addReference();
        leg->setMediaSession(media);
        peer->setMediaSession(media);

        delete leg;
        // the media processor's next DTMF round reaches the peer only; a
        // pointer left to the freed leg would be called here
        media->processDtmfEvents();
        fct_xchk(peer->dtmf_rounds == 1, "relay mode %d, %s leg destroyed: peer called %d times",
                 (int)modes[m], leg_is_a ? "A" : "B", peer->dtmf_rounds);

        delete peer;
        bool released = media->releaseReference();
        fct_xchk(released, "relay mode %d, %s leg destroyed: media session reference not released",
                 (int)modes[m], leg_is_a ? "A" : "B");
      }
    }
  }
  FCT_TEST_END();

  // A leg that changed its role after registration must still detach from the
  // slot it actually occupies. Resolving the leg from the stale a_leg flag
  // instead would release the peer's streams and leave a pointer to the freed
  // leg behind for the media processor to call.
  FCT_TEST_BGN(destroyed_leg_is_detached_by_identity_after_a_role_change) {
    fct_req(destroyed_legs_release_media);
    for (int m = 0; m < nb_modes; m++) {
      for (int side = 0; side < 2; side++) {
        bool leg_is_a = side == 0;
        TestLeg *leg = new TestLeg(leg_is_a);
        TestLeg *peer = new TestLeg(!leg_is_a);
        leg->setRtpRelayMode(modes[m]);
        peer->setRtpRelayMode(modes[m]);
        AmB2BMedia *media = new AmB2BMedia(leg_is_a ? leg : peer, leg_is_a ? peer : leg);
        media->addReference();
        leg->setMediaSession(media);
        peer->setMediaSession(media);

        leg->flipRole();

        delete leg;
        media->processDtmfEvents();
        fct_xchk(peer->dtmf_rounds == 1,
                 "relay mode %d, %s leg destroyed after role change: peer called %d times",
                 (int)modes[m], leg_is_a ? "A" : "B", peer->dtmf_rounds);

        delete peer;
        bool released = media->releaseReference();
        fct_xchk(released,
                 "relay mode %d, %s leg destroyed after role change: media session reference not released",
                 (int)modes[m], leg_is_a ? "A" : "B");
      }
    }
  }
  FCT_TEST_END();
}
FCTMF_SUITE_END();
