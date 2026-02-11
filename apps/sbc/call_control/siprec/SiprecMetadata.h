/*
 * SIPREC metadata XML builder (RFC 7865)
 *
 * Generates the recording metadata XML for SIPREC sessions.
 */

#ifndef _SIPREC_METADATA_H
#define _SIPREC_METADATA_H

#include <string>
#include <vector>
#include <ctime>

using std::string;
using std::vector;

#define SIPREC_METADATA_CONTENT_TYPE "application/rs-metadata+xml"
#define SIPREC_METADATA_NS "urn:ietf:params:xml:ns:recording:1"

struct SiprecParticipant {
  string participant_id; // base64 unique ID
  string aor;            // SIP address-of-record
  string name;           // display name (optional)
};

struct SiprecStream {
  string stream_id;       // base64 unique ID
  int    label;           // maps to SDP a=label
  string sender_id;       // participant_id of sender
  string receiver_id;     // participant_id of receiver
};

class SiprecMetadata {
  string session_id_;
  string sip_session_id_;  // original call-id
  time_t start_time_;
  time_t stop_time_;

  vector<SiprecParticipant> participants_;
  vector<SiprecStream>      streams_;

  static string generateId();
  static string timeToISO8601(time_t t);
  static string xmlEscape(const string& s);

public:
  SiprecMetadata();

  void setSession(const string& call_id);
  const string& getSessionId() const { return session_id_; }

  /** Add a participant, returns generated participant_id */
  string addParticipant(const string& aor, const string& name = "");

  /** Add a stream with label, returns generated stream_id */
  string addStream(int label, const string& sender_id,
                   const string& receiver_id);

  /** Build complete metadata snapshot XML */
  string buildXml() const;

  /** Build final metadata with stop-time for BYE */
  string buildFinalXml() const;

  /** Build metadata for hold (marks participant as inactive) */
  string buildHoldXml(const string& held_participant_id) const;
};

#endif
