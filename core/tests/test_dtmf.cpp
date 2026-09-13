#include "fct.h"

#include "log.h"

#include "AmDtmfDetector.h"
#include "AmSession.h"
#include "AmSipDialog.h"
#include "AmSipMsg.h"

#include <arpa/inet.h>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

// AmDtmfSink::postDtmfEvent() returns whether the sink took over the heap
// allocated event it was handed. AmSession only takes it while DTMF detection
// is enabled; a refused event stays with the caller, and AmDtmfDetector,
// AmSession::onSipRequest() and AmRtpStream::recvDtmfPacket() release it.
//
// The checks below are on the return value, on where an accepted event ends
// up, and on what reaches onDtmf() afterwards. Whether a call site actually
// releases a refused event cannot be seen from here, since the event is
// allocated inside the code under test: a missing delete in
// AmDtmfDetector::reportEvent() or AmSession::onSipRequest() shows up as a
// leak in the asan job only. AmRtpStream::recvDtmfPacket() is not exercised,
// it needs a stream with a negotiated telephone-event payload.

namespace {

std::string dtmf_relay_body(char key) { return std::string("Signal=") + key + "\r\nDuration=160\r\n"; }

AmDtmfEvent *sip_dtmf(char key) { return new AmSipDtmfEvent(dtmf_relay_body(key)); }

AmDtmfEvent *rtp_dtmf(unsigned char key, unsigned int ts) {
  dtmf_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  payload.event = key;
  payload.volume = 10; // AmRtpDtmfDetector ignores 55 and above
  payload.duration = htons(800);
  return new AmRtpDtmfEvent(&payload, 8000, ts);
}

AmSipRequest info_request(char key) {
  AmSipRequest req;
  req.method = SIP_METH_INFO;
  std::string body = dtmf_relay_body(key);
  req.body.parse("application/dtmf-relay", (const unsigned char *)body.c_str(), body.length());
  return req;
}

// hands evt to the sink the way the core call sites do: the caller releases
// whatever the sink refuses
bool post(AmDtmfSink &sink, AmDtmfEvent *evt) {
  if (sink.postDtmfEvent(evt)) {
    return true;
  }
  delete evt;
  return false;
}

// AmDtmfDetector::process() is private, AmDtmfEventQueue reaches it through
// AmEventHandler as well. The detector does not keep the raw event.
void feed(AmDtmfDetector &detector, AmDtmfEvent *evt) {
  std::unique_ptr<AmDtmfEvent> raw(evt);
  static_cast<AmEventHandler &>(detector).process(raw.get());
}

// records what AmDtmfDetector reports; when accepting, it owns the events
class RecordingSink : public AmDtmfSink {
public:
  explicit RecordingSink(bool accepting) : accepting(accepting) {}

  ~RecordingSink() override {
    for (size_t i = 0; i < taken.size(); i++) {
      delete taken[i];
    }
  }

  bool postDtmfEvent(AmDtmfEvent *evt) override {
    keys.push_back(evt->event());
    durations.push_back(evt->duration());
    if (!accepting) {
      return false;
    }
    taken.push_back(evt);
    return true;
  }

  bool accepting;
  std::vector<int> keys;
  std::vector<int> durations;
  std::vector<AmDtmfEvent *> taken;
};

// records replies instead of sending them, there is no transaction layer here
class RecordingDialog : public AmSipDialog {
public:
  int reply(const AmSipRequest &, unsigned int code, const std::string &, const AmMimeBody * = NULL,
            const std::string & = "", int = 0) override {
    codes.push_back(code);
    return 0;
  }

  std::vector<unsigned int> codes;
};

class DtmfSession : public AmSession {
public:
  DtmfSession() : AmSession(new RecordingDialog()) {}

  RecordingDialog *dialog() { return static_cast<RecordingDialog *>(dlg); }

  void onDtmf(int event, int duration) override {
    keys.push_back(event);
    durations.push_back(duration);
  }

  // runs the DTMF detector over the raw events queued so far, then delivers
  // what it reported to onDtmf()
  void deliverDtmf() {
    processDtmfEvents();
    processEvents();
  }

  std::vector<int> keys;
  std::vector<int> durations;
};

} // namespace

FCTMF_SUITE_BGN(test_dtmf) {

  // A key is reported once a different key arrives; the last key of each
  // sequence stays pending in the detector, which needs no timeout here.

  FCT_TEST_BGN(detector_hands_over_accepted_events) {
    RecordingSink sink(true);
    AmDtmfDetector detector(&sink);
    feed(detector, sip_dtmf('1'));
    fct_chk(sink.keys.empty());
    feed(detector, sip_dtmf('2'));
    fct_req(sink.taken.size() == 1);
    fct_chk_eq_int(sink.taken[0]->event(), 1);
    fct_chk_eq_int(sink.taken[0]->duration(), 160);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(detector_carries_on_after_refused_events) {
    RecordingSink sink(false);
    AmDtmfDetector detector(&sink);
    feed(detector, sip_dtmf('1'));
    feed(detector, sip_dtmf('2'));
    feed(detector, sip_dtmf('3'));
    // a refused report still clears the pending key: each key is offered once
    fct_req(sink.keys.size() == 2);
    fct_chk_eq_int(sink.keys[0], 1);
    fct_chk_eq_int(sink.keys[1], 2);
    fct_chk(sink.taken.empty());
  }
  FCT_TEST_END();

  FCT_TEST_BGN(session_refuses_dtmf_while_detection_is_disabled) {
    DtmfSession s;
    s.setDtmfDetectionEnabled(false);
    fct_chk(!post(s, sip_dtmf('1')));
    fct_chk(!post(s, rtp_dtmf(2, 1000)));
    fct_chk(!post(s, new AmDtmfEvent(3, 100)));
    fct_chk(!s.eventPending());

    // once detection is back on, only what was accepted reaches onDtmf()
    s.setDtmfDetectionEnabled(true);
    fct_chk(post(s, sip_dtmf('4')));
    fct_chk(post(s, sip_dtmf('5')));
    s.deliverDtmf();
    fct_req(s.keys.size() == 1);
    fct_chk_eq_int(s.keys[0], 4);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(session_takes_dtmf_while_detection_is_enabled) {
    DtmfSession s;
    fct_req(s.isDtmfDetectionEnabled());

    // an aggregated event goes straight into the session's event queue
    fct_chk(post(s, new AmDtmfEvent(7, 250)));
    fct_chk(s.eventPending());
    s.processEvents();
    fct_req(s.keys.size() == 1);
    fct_chk_eq_int(s.keys[0], 7);
    fct_chk_eq_int(s.durations[0], 250);

    // raw SIP INFO events go through the session's DTMF detector first
    fct_chk(post(s, sip_dtmf('1')));
    fct_chk(post(s, sip_dtmf('2')));
    fct_chk(!s.eventPending());
    s.deliverDtmf();
    fct_req(s.keys.size() == 2);
    fct_chk_eq_int(s.keys[1], 1);
    fct_chk_eq_int(s.durations[1], 160);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(session_takes_rtp_dtmf_while_detection_is_enabled) {
    DtmfSession s;
    fct_chk(post(s, rtp_dtmf(4, 1000)));
    fct_chk(post(s, rtp_dtmf(5, 2000)));
    fct_chk(!s.eventPending());
    s.deliverDtmf();
    fct_req(s.keys.size() == 1);
    fct_chk_eq_int(s.keys[0], 4);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(session_sip_info_dtmf) {
    DtmfSession s;
    s.setDtmfDetectionEnabled(false);
    s.onSipRequest(info_request('1'));
    // the INFO is answered whether or not its DTMF is taken
    fct_req(s.dialog()->codes.size() == 1);
    fct_chk_eq_int((int)s.dialog()->codes[0], 200);
    fct_chk(!s.eventPending());

    s.setDtmfDetectionEnabled(true);
    s.onSipRequest(info_request('2'));
    s.onSipRequest(info_request('3'));
    fct_req(s.dialog()->codes.size() == 3);
    fct_chk_eq_int((int)s.dialog()->codes[1], 200);
    fct_chk_eq_int((int)s.dialog()->codes[2], 200);
    s.deliverDtmf();
    fct_req(s.keys.size() == 1);
    fct_chk_eq_int(s.keys[0], 2);
  }
  FCT_TEST_END();
}
FCTMF_SUITE_END();
