/*
 * SIPREC SIP dialog management (RFC 7866).
 *
 * Manages an independent SIP dialog to the Session Recording Server (SRS).
 * Sends INVITE with multipart body (SDP + metadata XML), handles 200 OK
 * to extract SRS RTP ports, and sends BYE on call end.
 */

#ifndef _SIPREC_DIALOG_H
#define _SIPREC_DIALOG_H

#include "AmBasicSipDialog.h"
#include "AmEventQueue.h"
#include "AmSdp.h"
#include "SiprecMetadata.h"
#include "RtpForwarder.h"

#include <string>

using std::string;

class CCSiprec;

class SiprecDialog
  : public AmBasicSipEventHandler,
    public AmEventQueueInterface
{
public:
  enum State {
    Idle,
    Inviting,
    Recording,
    Terminating,
    Terminated
  };

private:
  AmBasicSipDialog dlg_;
  State state_;

  CCSiprec* module_;        // back-pointer for callbacks
  string    call_ltag_;     // SBC call leg tag (for state lookup)

  SiprecMetadata metadata_;

  RtpForwarder fwd_aleg_;   // A-leg (caller) RTP forwarder
  RtpForwarder fwd_bleg_;   // B-leg (callee) RTP forwarder

  string srs_uri_;          // SRS Request-URI
  string caller_uri_;       // participant AOR
  string callee_uri_;       // participant AOR
  string caller_id_;        // participant_id (base64)
  string callee_id_;        // participant_id (base64)
  string stream1_id_;       // A-leg stream_id
  string stream2_id_;       // B-leg stream_id

  unsigned short local_rtp_port_a_;  // allocated local RTP port for A-leg
  unsigned short local_rtp_port_b_;  // allocated local RTP port for B-leg

  // Codec for the SIPREC SDP offer
  string codec_name_;          // e.g. "PCMA", "PCMU"
  int    codec_payload_type_;  // RTP payload type
  int    codec_clock_rate_;    // clock rate

  // RTP profile for RS (RFC 7866 Section 12.2)
  int    transport_;           // TP_RTPAVP or TP_RTPSAVP

  /** Build the SDP offer with two sendonly m-lines */
  void buildSdpOffer(AmSdp& sdp, const AmSdp& cs_sdp);

  /** Build multipart MIME body (SDP + metadata) with Content-Disposition */
  void buildMultipartBody(AmMimeBody& body, const AmSdp& sdp,
                          const string& metadata_xml);

public:
  SiprecDialog(CCSiprec* module, const string& call_ltag);
  ~SiprecDialog();

  State getState() const { return state_; }

  /** Set the codec for the SIPREC SDP offer */
  void setCodec(const string& name, int payload_type, int clock_rate);

  /** Set the RTP transport profile (TP_RTPAVP or TP_RTPSAVP) */
  void setTransport(int transport);

  /** Initialize and send INVITE to SRS.
   *  @param srs_uri       SRS SIP URI
   *  @param caller_uri    caller AOR (From)
   *  @param callee_uri    callee AOR (To)
   *  @param cs_sdp        Communication Session SDP (for codec info)
   *  @param call_id       original call's Call-ID
   *  @return 0 on success */
  int startRecording(const string& srs_uri,
                     const string& caller_uri,
                     const string& callee_uri,
                     const AmSdp& cs_sdp,
                     const string& call_id);

  /** Send BYE with final metadata to SRS */
  void stopRecording();

  /** Send re-INVITE to SRS (hold/resume/codec change) */
  void updateRecording(const AmSdp& cs_sdp, bool on_hold);

  /** Forward an RTP packet to the appropriate SRS port.
   *  @param p      RTP packet
   *  @param is_aleg true if from A-leg, false if from B-leg */
  void forwardRtp(AmRtpPacket* p, bool is_aleg);

  // AmEventQueueInterface
  void postEvent(AmEvent* ev) override;

  // AmBasicSipEventHandler
  void onSipReply(const AmSipRequest& req,
                  const AmSipReply& reply,
                  AmBasicSipDialog::Status old_status) override;
  void onSipRequest(const AmSipRequest& req) override;
};

#endif
