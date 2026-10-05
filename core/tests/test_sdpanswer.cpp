#include "fct.h"

#include "log.h"

#include "AmConfig.h"
#include "AmSdp.h"
#include "AmSession.h"

#include <string>

#define CRLF "\r\n"

// AmSession::getSdpAnswer() rejects a media stream it cannot handle by echoing
// the offered type and transport with port 0. For a non-RTP transport the
// format list of an m= line lives in SdpMedia::fmt (parse_sdp_media() fills
// fmt, not payloads) and AmSdp::print() emits fmt for exactly those
// transports, so an answer that does not carry fmt over prints an m= line
// with no <fmt> at all - not a valid media description.
//
// The check is on the printed body, which is what goes on the wire.

namespace {

const char *OFFER_T38_ONLY = "v=0" CRLF "o=- 1 1 IN IP4 192.0.2.1" CRLF "s=-" CRLF "c=IN IP4 192.0.2.1" CRLF
                             "t=0 0" CRLF "m=image 6000 udptl t38" CRLF "a=T38FaxVersion:0" CRLF;

// getSdpAnswer() needs one media interface with an address to advertise, and
// getRtpInterface() looks the media interface up through the signaling one.
// Nothing here binds a socket: the rejected stream never asks for a port.
class ConfigGuard {
public:
  ConfigGuard() : saved_rtp(AmConfig::RTP_Ifs), saved_sip(AmConfig::SIP_Ifs) {
    AmConfig::RTP_interface rtp_if;
    rtp_if.name = "test";
    rtp_if.LocalIP = "192.0.2.9";
    AmConfig::RTP_Ifs.clear();
    AmConfig::RTP_Ifs.push_back(rtp_if);

    AmConfig::SIP_interface sip_if;
    sip_if.name = "test";
    sip_if.LocalIP = "192.0.2.9";
    sip_if.RtpInterface = 0;
    AmConfig::SIP_Ifs.clear();
    AmConfig::SIP_Ifs.push_back(sip_if);
  }

  ~ConfigGuard() {
    AmConfig::RTP_Ifs = saved_rtp;
    AmConfig::SIP_Ifs = saved_sip;
  }

private:
  vector<AmConfig::RTP_interface> saved_rtp;
  vector<AmConfig::SIP_interface> saved_sip;
};

// An offer with no usable audio stream makes getSdpAnswer() throw 488 after it
// has filled in the answer, which is all this test needs.
bool build_answer(const char *offer_body, AmSdp &answer) {
  AmSdp offer;
  if (offer.parse(offer_body))
    return false;

  AmSession s;
  try {
    s.getSdpAnswer(offer, answer);
  } catch (const AmSession::Exception &) {
    // expected: nothing in the offer could be accepted
  }
  return true;
}

} // namespace

FCTMF_SUITE_BGN(test_sdpanswer) {

  FCT_TEST_BGN(rejected_non_rtp_stream_keeps_its_format_list) {
    ConfigGuard cfg;

    AmSdp answer;
    fct_req(build_answer(OFFER_T38_ONLY, answer));

    fct_req(answer.media.size() == 1);
    fct_chk(answer.media[0].type == MT_IMAGE);
    fct_chk(answer.media[0].port == 0);
    fct_chk(answer.media[0].transport == TP_UDPTL);
    // the offered <fmt> has to survive into the answer, otherwise print()
    // has nothing to put after the transport
    fct_chk_eq_str(answer.media[0].fmt.c_str(), "t38");

    string body;
    answer.print(body);
    fct_chk(body.find("m=image 0 udptl t38" CRLF) != string::npos);
    // the malformed line this guards against
    fct_chk(body.find("m=image 0 udptl " CRLF) == string::npos);
    fct_chk(body.find("m=image 0 udptl" CRLF) == string::npos);
  }
  FCT_TEST_END();
}
FCTMF_SUITE_END();
