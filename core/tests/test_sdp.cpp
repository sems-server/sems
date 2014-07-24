#include "fct.h"

#include "log.h"

#include "AmSdp.h"

#define CRLF "\r\n"
#define LF "\n"

FCTMF_SUITE_BGN(test_sdp) {

    FCT_TEST_BGN(normal_sdp_ok) {
      AmSdp s;
      string sdp = 
	"v=0" CRLF
	"o=- 3615077380 3615077398 IN IP4 178.66.14.5" CRLF
	"s=-" CRLF
	"c=IN IP4 178.66.14.5" CRLF
	"t=0 0" CRLF
	"m=audio 21964 RTP/AVP 0 101" CRLF
	"a=sendrecv" CRLF
	"a=ptime:20" CRLF
	"a=rtpmap:0 PCMU/8000" CRLF
	"a=rtpmap:101 telephone-event/8000" CRLF
	"a=fmtp:101 0-15" CRLF ;

      fct_chk(!s.parse(sdp.c_str()));
      fct_chk(s.version==0);
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
      fct_chk(s.media[0].payloads.size()==2);
      fct_chk(s.media[0].payloads[0].payload_type==0);
      fct_chk(s.media[0].payloads[1].payload_type==101);
      fct_chk(s.media[0].payloads[0].encoding_name=="PCMU");
      fct_chk(s.media[0].payloads[1].encoding_name=="telephone-event");
    } FCT_TEST_END();

    FCT_TEST_BGN(sdp_LF_no_CRLF) {
      AmSdp s;
      string sdp = 
	"v=0" LF
	"o=- 3615077380 3615077398 IN IP4 178.66.14.5" LF
	"s=-" LF
	"c=IN IP4 178.66.14.5" LF
	"t=0 0" LF
	"m=audio 21964 RTP/AVP 0 101" LF
	"a=sendrecv" LF
	"a=ptime:20" LF
	"a=rtpmap:0 PCMU/8000" LF
	"a=rtpmap:101 telephone-event/8000" LF
	"a=fmtp:101 0-15" LF ;

      fct_chk(!s.parse(sdp.c_str()));
      fct_chk(s.version==0);
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
      fct_chk(s.media[0].payloads.size()==2);
      fct_chk(s.media[0].payloads[0].payload_type==0);
      fct_chk(s.media[0].payloads[1].payload_type==101);
      fct_chk(s.media[0].payloads[0].encoding_name=="PCMU");
      fct_chk(s.media[0].payloads[1].encoding_name=="telephone-event");
    } FCT_TEST_END();

} FCTMF_SUITE_END();
