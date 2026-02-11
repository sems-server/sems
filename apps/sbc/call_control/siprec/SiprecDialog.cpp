/*
 * SIPREC SIP dialog management (RFC 7866).
 */

#include "SiprecDialog.h"
#include "CCSiprec.h"

#include "AmConfig.h"
#include "AmSession.h"
#include "AmMimeBody.h"
#include "AmSipMsg.h"
#include "AmEventDispatcher.h"
#include "AmSipEvent.h"
#include "sip/defs.h"
#include "log.h"

#include <cstring>

SiprecDialog::SiprecDialog(CCSiprec* module, const string& call_ltag)
  : dlg_(this),
    state_(Idle),
    module_(module),
    call_ltag_(call_ltag),
    local_rtp_port_a_(0),
    local_rtp_port_b_(0),
    codec_name_("PCMA"),
    codec_payload_type_(8),
    codec_clock_rate_(8000),
    transport_(TP_RTPAVP)
{
}

void SiprecDialog::setCodec(const string& name, int payload_type, int clock_rate) {
  codec_name_ = name;
  codec_payload_type_ = payload_type;
  codec_clock_rate_ = clock_rate;
}

void SiprecDialog::setTransport(int transport) {
  transport_ = transport;
}

SiprecDialog::~SiprecDialog() {
  if (state_ != Terminated && state_ != Idle) {
    DBG("SIPREC: dialog destroyed in state %d, sending TEARDOWN\n", state_);
    stopRecording();
  }

  string ltag = dlg_.getLocalTag();
  if (!ltag.empty()) {
    AmEventDispatcher::instance()->delEventQueue(ltag);
  }
}

void SiprecDialog::buildSdpOffer(AmSdp& sdp, const AmSdp& cs_sdp) {
  // Build SDP with two sendonly m-lines for recording

  sdp.version = 0;
  sdp.origin.user = "sems-siprec";
  sdp.origin.sessId = (unsigned int)time(NULL);
  sdp.origin.sessV = 1;
  sdp.sessionName = "SIPREC Recording";

  // Use first available SIP interface IP for connection
  string local_ip;
  if (!AmConfig::SIP_Ifs.empty()) {
    local_ip = AmConfig::SIP_Ifs[0].getIP();
  }
  if (local_ip.empty()) local_ip = "0.0.0.0";

  sdp.conn.address = local_ip;
  sdp.conn.network = NT_IN;
  sdp.conn.addrType = AT_V4;
  sdp.origin.conn = sdp.conn;

  // Use configured codec (from cc_siprec.conf)
  int payload_type = codec_payload_type_;
  string codec_name = codec_name_;
  int clock_rate = codec_clock_rate_;

  // Override with CS SDP codec if available
  for (size_t i = 0; i < cs_sdp.media.size(); i++) {
    if (cs_sdp.media[i].type == MT_AUDIO && !cs_sdp.media[i].payloads.empty()) {
      const SdpPayload& p = cs_sdp.media[i].payloads[0];
      payload_type = p.payload_type;
      if (!p.encoding_name.empty()) codec_name = p.encoding_name;
      if (p.clock_rate > 0) clock_rate = p.clock_rate;
      break;
    }
  }

  // m=audio line 1: A-leg (caller) audio, label 1
  // Uses allocated local port for symmetric RTP (RFC 7866 Section 8.1.8)
  SdpMedia m1;
  m1.type = MT_AUDIO;
  m1.port = local_rtp_port_a_;
  m1.transport = (TransProt)transport_;
  m1.send = true;
  m1.recv = false;     // sendonly

  SdpPayload p1;
  p1.payload_type = payload_type;
  p1.encoding_name = codec_name;
  p1.clock_rate = clock_rate;
  m1.payloads.push_back(p1);

  m1.attributes.push_back(SdpAttribute("label", "1"));
  sdp.media.push_back(m1);

  // m=audio line 2: B-leg (callee) audio, label 2
  SdpMedia m2;
  m2.type = MT_AUDIO;
  m2.port = local_rtp_port_b_;
  m2.transport = (TransProt)transport_;
  m2.send = true;
  m2.recv = false;     // sendonly

  SdpPayload p2;
  p2.payload_type = payload_type;
  p2.encoding_name = codec_name;
  p2.clock_rate = clock_rate;
  m2.payloads.push_back(p2);

  m2.attributes.push_back(SdpAttribute("label", "2"));
  sdp.media.push_back(m2);
}

void SiprecDialog::buildMultipartBody(AmMimeBody& body, const AmSdp& sdp,
                                       const string& metadata_xml) {
  // Part 1: SDP
  AmMimeBody* sdp_part = body.addPart(SIP_APPLICATION_SDP);
  if (sdp_part) {
    string sdp_str;
    sdp.print(sdp_str);
    sdp_part->setPayload((const unsigned char*)sdp_str.c_str(),
                         sdp_str.length());
  }

  // Part 2: SIPREC metadata XML
  // RFC 7866 Section 9.1: Content-Disposition: recording-session is REQUIRED
  AmMimeBody* meta_part = body.addPart(SIPREC_METADATA_CONTENT_TYPE);
  if (meta_part) {
    meta_part->setHeaders("Content-Disposition: recording-session\r\n");
    meta_part->setPayload((const unsigned char*)metadata_xml.c_str(),
                          metadata_xml.length());
  }
}

int SiprecDialog::startRecording(const string& srs_uri,
                                  const string& caller_uri,
                                  const string& callee_uri,
                                  const AmSdp& cs_sdp,
                                  const string& call_id) {
  srs_uri_ = srs_uri;
  caller_uri_ = caller_uri;
  callee_uri_ = callee_uri;

  // Allocate local RTP port pairs for symmetric RTP (RFC 7866 Section 8.1.8)
  // Each allocation returns an even port; RTCP is port+1
  local_rtp_port_a_ = SiprecPortAllocator::allocate();
  local_rtp_port_b_ = SiprecPortAllocator::allocate();
  if (local_rtp_port_a_ == 0 || local_rtp_port_b_ == 0) {
    ERROR("SIPREC: failed to allocate RTP ports\n");
    return -1;
  }

  DBG("SIPREC: allocated local RTP ports: A-leg=%u, B-leg=%u\n",
      local_rtp_port_a_, local_rtp_port_b_);

  // Initialize recording dialog
  dlg_.setLocalTag(AmSession::getNewId());
  dlg_.setCallid(AmSession::getNewId());

  // Register with event dispatcher so SIP replies are routed to us
  AmEventDispatcher::instance()->addEventQueue(dlg_.getLocalTag(), this);

  string local_ip;
  if (!AmConfig::SIP_Ifs.empty())
    local_ip = AmConfig::SIP_Ifs[0].getIP();
  if (local_ip.empty()) local_ip = "localhost";

  dlg_.setLocalParty("sip:siprec@" + local_ip);
  dlg_.setRemoteParty(srs_uri_);
  dlg_.setRemoteUri(srs_uri_);

  // RFC 7866 Section 6.1.1: SRC MUST include +sip.src feature tag in Contact
  dlg_.setContactParams("+sip.src");

  // Build metadata
  metadata_.setSession(call_id);
  caller_id_ = metadata_.addParticipant(caller_uri_);
  callee_id_ = metadata_.addParticipant(callee_uri_);

  // Stream 1: caller sends (A-leg audio), label 1
  stream1_id_ = metadata_.addStream(1, caller_id_, callee_id_);
  // Stream 2: callee sends (B-leg audio), label 2
  stream2_id_ = metadata_.addStream(2, callee_id_, caller_id_);

  // Build SDP offer
  AmSdp sdp_offer;
  buildSdpOffer(sdp_offer, cs_sdp);

  // Build multipart body
  AmMimeBody body;
  string xml = metadata_.buildXml();
  buildMultipartBody(body, sdp_offer, xml);

  // RFC 7866 Section 6.1.1: SRC MUST include siprec option tag in Require
  string hdrs = "Require: siprec\r\n";

  // Send INVITE
  state_ = Inviting;
  int ret = dlg_.sendRequest(SIP_METH_INVITE, &body, hdrs);
  if (ret != 0) {
    ERROR("SIPREC: failed to send INVITE to SRS '%s'\n", srs_uri_.c_str());
    state_ = Terminated;
    return -1;
  }

  INFO("SIPREC: sent INVITE to SRS '%s' for call '%s'\n",
       srs_uri_.c_str(), call_id.c_str());
  return 0;
}

void SiprecDialog::stopRecording() {
  if (state_ != Recording && state_ != Inviting) {
    DBG("SIPREC: stopRecording called in state %d, ignoring\n", state_);
    return;
  }

  // Stop RTP forwarding
  fwd_aleg_.stop();
  fwd_bleg_.stop();

  if (state_ == Inviting) {
    // RFC 3261: send CANCEL for pending INVITE
    DBG("SIPREC: sending CANCEL for pending INVITE to SRS\n");
    int ret = dlg_.sendRequest(SIP_METH_CANCEL);
    if (ret != 0) {
      WARN("SIPREC: failed to send CANCEL to SRS\n");
    }
    AmEventDispatcher::instance()->delEventQueue(dlg_.getLocalTag());
    state_ = Terminated;
    return;
  }

  // Build final metadata for BYE body
  string final_xml = metadata_.buildFinalXml();
  AmMimeBody body;
  body.setPayload((const unsigned char*)final_xml.c_str(),
                  final_xml.length());

  string hdrs = "Content-Type: " SIPREC_METADATA_CONTENT_TYPE "\r\n"
                "Content-Disposition: recording-session\r\n";

  state_ = Terminating;
  int ret = dlg_.sendRequest(SIP_METH_BYE, &body, hdrs);
  if (ret != 0) {
    WARN("SIPREC: failed to send BYE to SRS\n");
    state_ = Terminated;
  } else {
    INFO("SIPREC: sent BYE to SRS\n");
  }
}

void SiprecDialog::updateRecording(const AmSdp& cs_sdp, bool on_hold) {
  if (state_ != Recording) return;

  // Build updated SDP
  AmSdp sdp;
  buildSdpOffer(sdp, cs_sdp);

  if (on_hold) {
    // Set both m-lines to inactive (RFC 7866 Section 7.1.1.1)
    for (size_t i = 0; i < sdp.media.size(); i++) {
      sdp.media[i].send = false;
      sdp.media[i].recv = false;
    }
  }

  // Build multipart body with metadata
  AmMimeBody body;
  string xml = on_hold ? metadata_.buildHoldXml(caller_id_) : metadata_.buildXml();
  buildMultipartBody(body, sdp, xml);

  string hdrs = "Require: siprec\r\n";
  int ret = dlg_.sendRequest(SIP_METH_INVITE, &body, hdrs);
  if (ret != 0) {
    WARN("SIPREC: failed to send re-INVITE to SRS\n");
  }
}

void SiprecDialog::forwardRtp(AmRtpPacket* p, bool is_aleg) {
  if (state_ != Recording) return;

  if (is_aleg)
    fwd_aleg_.sendPacket(p);
  else
    fwd_bleg_.sendPacket(p);
}

void SiprecDialog::postEvent(AmEvent* ev) {
  AmSipReplyEvent* reply_ev = dynamic_cast<AmSipReplyEvent*>(ev);
  if (reply_ev) {
    dlg_.onRxReply(reply_ev->reply);
    return;
  }

  AmSipRequestEvent* req_ev = dynamic_cast<AmSipRequestEvent*>(ev);
  if (req_ev) {
    dlg_.onRxRequest(req_ev->req);
    return;
  }

  WARN("SIPREC: unhandled event in postEvent\n");
}

void SiprecDialog::onSipReply(const AmSipRequest& req,
                               const AmSipReply& reply,
                               AmBasicSipDialog::Status old_status) {
  DBG("SIPREC: received %d %s for %s\n",
      reply.code, reply.reason.c_str(), req.method.c_str());

  if (req.method == SIP_METH_INVITE) {
    if (reply.code >= 200 && reply.code < 300) {
      // 200 OK - parse SDP answer to get SRS RTP ports
      if (reply.body.getLen()) {
        const AmMimeBody* sdp_body = reply.body.hasContentType(SIP_APPLICATION_SDP);
        if (sdp_body) {
          AmSdp answer;
          if (answer.parse((const char*)sdp_body->getPayload()) == 0) {
            string srs_ip = answer.conn.address;

            // Local IP for binding forwarder sockets
            string local_ip;
            if (!AmConfig::SIP_Ifs.empty())
              local_ip = AmConfig::SIP_Ifs[0].getIP();
            if (local_ip.empty()) local_ip = "0.0.0.0";

            // Extract port for each m-line
            if (answer.media.size() >= 2) {
              unsigned short port_a = answer.media[0].port;
              unsigned short port_b = answer.media[1].port;

              // Per-media connection address overrides session-level
              string srs_ip_a = srs_ip;
              if (!answer.media[0].conn.address.empty())
                srs_ip_a = answer.media[0].conn.address;

              string srs_ip_b = srs_ip;
              if (!answer.media[1].conn.address.empty())
                srs_ip_b = answer.media[1].conn.address;

              DBG("SIPREC: SRS RTP ports: A-leg=%u, B-leg=%u, IP=%s\n",
                  port_a, port_b, srs_ip.c_str());

              // Initialize and start RTP forwarders with local bind
              // (symmetric RTP) and RTCP sockets
              if (fwd_aleg_.init(local_ip, local_rtp_port_a_,
                                 srs_ip_a, port_a) == 0)
                fwd_aleg_.start();
              else
                ERROR("SIPREC: failed to init A-leg RTP forwarder\n");

              if (fwd_bleg_.init(local_ip, local_rtp_port_b_,
                                 srs_ip_b, port_b) == 0)
                fwd_bleg_.start();
              else
                ERROR("SIPREC: failed to init B-leg RTP forwarder\n");

              state_ = Recording;
              INFO("SIPREC: recording started, forwarding RTP to %s:%u/%u\n",
                   srs_ip.c_str(), port_a, port_b);
            } else {
              ERROR("SIPREC: SRS SDP answer has < 2 m-lines\n");
              state_ = Terminated;
            }
          } else {
            ERROR("SIPREC: failed to parse SRS SDP answer\n");
            state_ = Terminated;
          }
        }
      }

      // Update dialog state from 200 OK (AmBasicSipDialog doesn't do this)
      dlg_.setRemoteTag(reply.to_tag);
      dlg_.setRouteSet(reply.route);

      // Send ACK (must use same CSeq as INVITE)
      unsigned int saved_cseq = dlg_.cseq;
      dlg_.cseq = reply.cseq;
      dlg_.sendRequest("ACK");
      dlg_.cseq = saved_cseq;

    } else if (reply.code >= 300) {
      // SRS rejected recording
      WARN("SIPREC: SRS rejected recording: %d %s\n",
           reply.code, reply.reason.c_str());
      state_ = Terminated;
    }
  } else if (req.method == SIP_METH_BYE) {
    if (reply.code >= 200) {
      state_ = Terminated;
      DBG("SIPREC: recording session terminated\n");
    }
  }
}

void SiprecDialog::onSipRequest(const AmSipRequest& req) {
  DBG("SIPREC: received %s from SRS\n", req.method.c_str());

  if (req.method == SIP_METH_BYE) {
    // SRS is terminating the recording session
    INFO("SIPREC: SRS sent BYE, stopping recording\n");
    fwd_aleg_.stop();
    fwd_bleg_.stop();
    state_ = Terminated;

    dlg_.reply(req, 200, "OK");
  }
}
