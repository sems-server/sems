#include "fct.h"

#include "log.h"

#include "AmSdp.h"

#define CRLF "\r\n"
#define LF "\n"

FCTMF_SUITE_BGN(test_sdp) {

  FCT_TEST_BGN(normal_sdp_ok) {
    AmSdp s;
    string sdp =
        "v=0" CRLF "o=- 3615077380 3615077398 IN IP4 178.66.14.5" CRLF "s=-" CRLF "c=IN IP4 178.66.14.5" CRLF
        "t=0 0" CRLF "m=audio 21964 RTP/AVP 0 101" CRLF "a=sendrecv" CRLF "a=ptime:20" CRLF
        "a=rtpmap:0 PCMU/8000" CRLF "a=rtpmap:101 telephone-event/8000" CRLF "a=fmtp:101 0-15" CRLF;

    fct_chk(!s.parse(sdp.c_str()));
    fct_chk(s.version == 0);
    fct_chk(s.origin.user == "-");
    fct_chk(s.origin.sessId == 3615077380);
    fct_chk(s.origin.sessV == 3615077398);
    fct_chk(s.origin.conn.address == "178.66.14.5");
    fct_chk(s.origin.conn.network == NT_IN);
    fct_chk(s.origin.conn.addrType == AT_V4);

    fct_chk(s.conn.address == "178.66.14.5");
    fct_chk(s.conn.network == NT_IN);
    fct_chk(s.conn.addrType == AT_V4);

    fct_chk(s.media.size() == 1);
    fct_chk(s.media[0].type == MT_AUDIO);
    fct_chk(s.media[0].port == 21964);
    fct_chk(s.media[0].transport == TP_RTPAVP);
    fct_chk(s.media[0].payloads.size() == 2);
    fct_chk(s.media[0].payloads[0].payload_type == 0);
    fct_chk(s.media[0].payloads[1].payload_type == 101);
    fct_chk(s.media[0].payloads[0].encoding_name == "PCMU");
    fct_chk(s.media[0].payloads[1].encoding_name == "telephone-event");
  }
  FCT_TEST_END();

  FCT_TEST_BGN(sdp_LF_no_CRLF) {
    AmSdp s;
    string sdp = "v=0" LF "o=- 3615077380 3615077398 IN IP4 178.66.14.5" LF "s=-" LF "c=IN IP4 178.66.14.5" LF
                 "t=0 0" LF "m=audio 21964 RTP/AVP 0 101" LF "a=sendrecv" LF "a=ptime:20" LF
                 "a=rtpmap:0 PCMU/8000" LF "a=rtpmap:101 telephone-event/8000" LF "a=fmtp:101 0-15" LF;

    fct_chk(!s.parse(sdp.c_str()));
    fct_chk(s.version == 0);
    fct_chk(s.origin.user == "-");
    fct_chk(s.origin.sessId == 3615077380);
    fct_chk(s.origin.sessV == 3615077398);
    fct_chk(s.origin.conn.address == "178.66.14.5");
    fct_chk(s.origin.conn.network == NT_IN);
    fct_chk(s.origin.conn.addrType == AT_V4);

    fct_chk(s.conn.address == "178.66.14.5");
    fct_chk(s.conn.network == NT_IN);
    fct_chk(s.conn.addrType == AT_V4);

    fct_chk(s.media.size() == 1);
    fct_chk(s.media[0].type == MT_AUDIO);
    fct_chk(s.media[0].port == 21964);
    fct_chk(s.media[0].transport == TP_RTPAVP);
    fct_chk(s.media[0].payloads.size() == 2);
    fct_chk(s.media[0].payloads[0].payload_type == 0);
    fct_chk(s.media[0].payloads[1].payload_type == 101);
    fct_chk(s.media[0].payloads[0].encoding_name == "PCMU");
    fct_chk(s.media[0].payloads[1].encoding_name == "telephone-event");
  }
  FCT_TEST_END();

  /* A malformed token in the m= format list must not contribute a payload.
     parse_sdp_media() used to feed an unchecked str2i() result into
     payload.payload_type, so a bogus token either duplicated the previously
     parsed type or - for the very first token - published an indeterminate
     value. */

  FCT_TEST_BGN(m_line_skips_non_numeric_payload) {
    AmSdp s;
    string sdp =
        "v=0" CRLF "o=- 1 1 IN IP4 10.0.0.1" CRLF "s=-" CRLF "c=IN IP4 10.0.0.1" CRLF
        "t=0 0" CRLF "m=audio 21964 RTP/AVP 0 bogus 101" CRLF
        "a=rtpmap:0 PCMU/8000" CRLF "a=rtpmap:101 telephone-event/8000" CRLF;

    fct_chk(!s.parse(sdp.c_str()));
    fct_chk(s.media.size() == 1);
    fct_chk(s.media[0].payloads.size() == 2);
    fct_chk(s.media[0].payloads[0].payload_type == 0);
    fct_chk(s.media[0].payloads[1].payload_type == 101);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(m_line_skips_leading_non_numeric_payload) {
    AmSdp s;
    string sdp =
        "v=0" CRLF "o=- 1 1 IN IP4 10.0.0.1" CRLF "s=-" CRLF "c=IN IP4 10.0.0.1" CRLF
        "t=0 0" CRLF "m=audio 21964 RTP/AVP bogus 8" CRLF "a=rtpmap:8 PCMA/8000" CRLF;

    fct_chk(!s.parse(sdp.c_str()));
    fct_chk(s.media.size() == 1);
    fct_chk(s.media[0].payloads.size() == 1);
    fct_chk(s.media[0].payloads[0].payload_type == 8);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(m_line_skips_trailing_non_numeric_payload) {
    AmSdp s;
    string sdp =
        "v=0" CRLF "o=- 1 1 IN IP4 10.0.0.1" CRLF "s=-" CRLF "c=IN IP4 10.0.0.1" CRLF
        "t=0 0" CRLF "m=audio 21964 RTP/AVP 8 bogus" CRLF "a=rtpmap:8 PCMA/8000" CRLF;

    fct_chk(!s.parse(sdp.c_str()));
    fct_chk(s.media.size() == 1);
    fct_chk(s.media[0].payloads.size() == 1);
    fct_chk(s.media[0].payloads[0].payload_type == 8);
  }
  FCT_TEST_END();

  /* RFC 3551: an RTP payload type is 7 bit. Anything else would be stored
     into the signed payload_type and re-emitted by AmSdp::print(). */

  FCT_TEST_BGN(m_line_skips_out_of_range_payload) {
    AmSdp s;
    string sdp =
        "v=0" CRLF "o=- 1 1 IN IP4 10.0.0.1" CRLF "s=-" CRLF "c=IN IP4 10.0.0.1" CRLF
        "t=0 0" CRLF "m=audio 21964 RTP/AVP 0 128 4294967295 101" CRLF
        "a=rtpmap:0 PCMU/8000" CRLF "a=rtpmap:101 telephone-event/8000" CRLF;

    fct_chk(!s.parse(sdp.c_str()));
    fct_chk(s.media.size() == 1);
    fct_chk(s.media[0].payloads.size() == 2);
    fct_chk(s.media[0].payloads[0].payload_type == 0);
    fct_chk(s.media[0].payloads[1].payload_type == 101);
  }
  FCT_TEST_END();

  /* All payload types unusable -> empty list, not a list of junk. */

  FCT_TEST_BGN(m_line_all_payloads_malformed) {
    AmSdp s;
    string sdp =
        "v=0" CRLF "o=- 1 1 IN IP4 10.0.0.1" CRLF "s=-" CRLF "c=IN IP4 10.0.0.1" CRLF
        "t=0 0" CRLF "m=audio 21964 RTP/AVP bogus junk" CRLF;

    fct_chk(!s.parse(sdp.c_str()));
    fct_chk(s.media.size() == 1);
    fct_chk(s.media[0].payloads.empty());
  }
  FCT_TEST_END();

  /* The stale payload_type is not scoped to a single m= line: parse_sdp_media()
     reads whatever the uninitialised stack slot holds, which in practice is a
     leftover from an earlier, unrelated SDP parsed on the same thread. Parsing
     a body that announces 101 and then one whose first <fmt> is malformed used
     to yield "101 8" for the second body. */

  FCT_TEST_BGN(m_line_bad_payload_does_not_inherit_earlier_parse) {
    string head =
        "v=0" CRLF "o=- 1 1 IN IP4 10.0.0.1" CRLF "s=-" CRLF "c=IN IP4 10.0.0.1" CRLF "t=0 0" CRLF;

    AmSdp first;
    fct_chk(!first.parse((head + "m=audio 21964 RTP/AVP 101" CRLF
                                 "a=rtpmap:101 telephone-event/8000" CRLF).c_str()));
    fct_chk(first.media[0].payloads.size() == 1);
    fct_chk(first.media[0].payloads[0].payload_type == 101);

    AmSdp second;
    fct_chk(!second.parse((head + "m=audio 21964 RTP/AVP bogus 8" CRLF
                                  "a=rtpmap:8 PCMA/8000" CRLF).c_str()));
    fct_chk(second.media.size() == 1);
    fct_chk(second.media[0].payloads.size() == 1);
    fct_chk(second.media[0].payloads[0].payload_type == 8);
  }
  FCT_TEST_END();
}
FCTMF_SUITE_END();
