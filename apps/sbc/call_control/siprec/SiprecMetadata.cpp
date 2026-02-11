/*
 * SIPREC metadata XML builder (RFC 7865)
 *
 * Generates recording metadata XML compliant with RFC 7865 Section 5-7,
 * including session, participant, stream, sessionrecordingassoc,
 * participantsessionassoc, and participantstreamassoc elements.
 */

#include "SiprecMetadata.h"
#include "AmSession.h"
#include "AmUtils.h"

#include <sstream>
#include <cstring>

// Simple base64 encoding for IDs (RFC 7865 requires base64Binary for IDs)
static string base64Encode(const unsigned char* data, size_t len) {
  static const char b64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  string result;
  result.reserve(((len + 2) / 3) * 4);

  for (size_t i = 0; i < len; i += 3) {
    unsigned int n = ((unsigned int)data[i]) << 16;
    if (i + 1 < len) n |= ((unsigned int)data[i + 1]) << 8;
    if (i + 2 < len) n |= (unsigned int)data[i + 2];

    result += b64[(n >> 18) & 0x3f];
    result += b64[(n >> 12) & 0x3f];
    result += (i + 1 < len) ? b64[(n >> 6) & 0x3f] : '=';
    result += (i + 2 < len) ? b64[n & 0x3f] : '=';
  }
  return result;
}

string SiprecMetadata::generateId() {
  // Generate a unique 16-byte ID and base64 encode it
  string raw_id = AmSession::getNewId(); // UUID-like string
  // Use first 16 bytes of the hash
  unsigned char buf[16];
  memset(buf, 0, sizeof(buf));
  for (size_t i = 0; i < raw_id.size() && i < 16; i++) {
    buf[i] = (unsigned char)raw_id[i];
  }
  return base64Encode(buf, 16);
}

string SiprecMetadata::timeToISO8601(time_t t) {
  char buf[64];
  struct tm tm_val;
  gmtime_r(&t, &tm_val);
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_val);
  return string(buf);
}

string SiprecMetadata::xmlEscape(const string& s) {
  string result;
  result.reserve(s.size());
  for (size_t i = 0; i < s.size(); i++) {
    switch (s[i]) {
      case '&':  result += "&amp;";  break;
      case '<':  result += "&lt;";   break;
      case '>':  result += "&gt;";   break;
      case '"':  result += "&quot;"; break;
      case '\'': result += "&apos;"; break;
      default:   result += s[i];     break;
    }
  }
  return result;
}

SiprecMetadata::SiprecMetadata()
  : start_time_(0), stop_time_(0)
{
  session_id_ = generateId();
}

void SiprecMetadata::setSession(const string& call_id) {
  sip_session_id_ = call_id;
  start_time_ = time(NULL);
}

string SiprecMetadata::addParticipant(const string& aor, const string& name) {
  SiprecParticipant p;
  p.participant_id = generateId();
  p.aor = aor;
  p.name = name;
  participants_.push_back(p);
  return p.participant_id;
}

string SiprecMetadata::addStream(int label, const string& sender_id,
                                 const string& receiver_id) {
  SiprecStream s;
  s.stream_id = generateId();
  s.label = label;
  s.sender_id = sender_id;
  s.receiver_id = receiver_id;
  streams_.push_back(s);
  return s.stream_id;
}

string SiprecMetadata::buildXml() const {
  std::ostringstream xml;

  xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
      << "<recording xmlns=\"" << SIPREC_METADATA_NS << "\">\r\n";

  // Session element (RFC 7865 Section 6.2)
  xml << "  <session session_id=\"" << session_id_ << "\">\r\n"
      << "    <sipSessionID>" << xmlEscape(sip_session_id_)
      << "</sipSessionID>\r\n";
  if (start_time_)
    xml << "    <start-time>" << timeToISO8601(start_time_)
        << "</start-time>\r\n";
  xml << "  </session>\r\n";

  // Participant elements (RFC 7865 Section 6.5)
  for (size_t i = 0; i < participants_.size(); i++) {
    const SiprecParticipant& p = participants_[i];
    xml << "  <participant participant_id=\"" << p.participant_id << "\">\r\n"
        << "    <nameID aor=\"" << xmlEscape(p.aor) << "\"";
    if (!p.name.empty())
      xml << ">\r\n      <name>" << xmlEscape(p.name) << "</name>\r\n"
          << "    </nameID>\r\n";
    else
      xml << "/>\r\n";
    xml << "  </participant>\r\n";
  }

  // Stream elements (RFC 7865 Section 6.6)
  for (size_t i = 0; i < streams_.size(); i++) {
    const SiprecStream& s = streams_[i];
    xml << "  <stream stream_id=\"" << s.stream_id
        << "\" session_id=\"" << session_id_ << "\">\r\n"
        << "    <label>" << s.label << "</label>\r\n"
        << "  </stream>\r\n";
  }

  // Session-Recording association (RFC 7865 Section 6.4)
  string assoc_time = start_time_ ? timeToISO8601(start_time_) : "";
  xml << "  <sessionrecordingassoc session_id=\"" << session_id_ << "\">\r\n";
  if (!assoc_time.empty())
    xml << "    <associate-time>" << assoc_time << "</associate-time>\r\n";
  xml << "  </sessionrecordingassoc>\r\n";

  // Participant-Session associations (RFC 7865 Section 6.7)
  for (size_t i = 0; i < participants_.size(); i++) {
    const SiprecParticipant& p = participants_[i];
    xml << "  <participantsessionassoc participant_id=\""
        << p.participant_id << "\" session_id=\"" << session_id_ << "\">\r\n";
    if (!assoc_time.empty())
      xml << "    <associate-time>" << assoc_time << "</associate-time>\r\n";
    xml << "  </participantsessionassoc>\r\n";
  }

  // Participant-Stream associations (RFC 7865 Section 6.8)
  for (size_t i = 0; i < participants_.size(); i++) {
    const SiprecParticipant& p = participants_[i];
    xml << "  <participantstreamassoc participant_id=\""
        << p.participant_id << "\">\r\n";

    for (size_t j = 0; j < streams_.size(); j++) {
      const SiprecStream& s = streams_[j];
      // RFC 7865: <send> and <recv> contain stream_id as text content
      if (s.sender_id == p.participant_id)
        xml << "    <send>" << s.stream_id << "</send>\r\n";
      if (s.receiver_id == p.participant_id)
        xml << "    <recv>" << s.stream_id << "</recv>\r\n";
    }

    if (!assoc_time.empty())
      xml << "    <associate-time>" << assoc_time << "</associate-time>\r\n";
    xml << "  </participantstreamassoc>\r\n";
  }

  xml << "</recording>\r\n";
  return xml.str();
}

string SiprecMetadata::buildFinalXml() const {
  std::ostringstream xml;
  time_t now = time(NULL);
  string stop_str = timeToISO8601(now);

  xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
      << "<recording xmlns=\"" << SIPREC_METADATA_NS << "\">\r\n";

  // Session with stop-time
  xml << "  <session session_id=\"" << session_id_ << "\">\r\n"
      << "    <sipSessionID>" << xmlEscape(sip_session_id_)
      << "</sipSessionID>\r\n";
  if (start_time_)
    xml << "    <start-time>" << timeToISO8601(start_time_)
        << "</start-time>\r\n";
  xml << "    <stop-time>" << stop_str << "</stop-time>\r\n"
      << "  </session>\r\n";

  // Participants (same as initial)
  for (size_t i = 0; i < participants_.size(); i++) {
    const SiprecParticipant& p = participants_[i];
    xml << "  <participant participant_id=\"" << p.participant_id << "\">\r\n"
        << "    <nameID aor=\"" << xmlEscape(p.aor) << "\"/>\r\n"
        << "  </participant>\r\n";
  }

  // Streams (same)
  for (size_t i = 0; i < streams_.size(); i++) {
    const SiprecStream& s = streams_[i];
    xml << "  <stream stream_id=\"" << s.stream_id
        << "\" session_id=\"" << session_id_ << "\">\r\n"
        << "    <label>" << s.label << "</label>\r\n"
        << "  </stream>\r\n";
  }

  // Session-Recording association with disassociate-time
  string assoc_time = start_time_ ? timeToISO8601(start_time_) : "";
  xml << "  <sessionrecordingassoc session_id=\"" << session_id_ << "\">\r\n";
  if (!assoc_time.empty())
    xml << "    <associate-time>" << assoc_time << "</associate-time>\r\n";
  xml << "    <disassociate-time>" << stop_str << "</disassociate-time>\r\n"
      << "  </sessionrecordingassoc>\r\n";

  // Participant-Session associations with disassociate-time
  for (size_t i = 0; i < participants_.size(); i++) {
    const SiprecParticipant& p = participants_[i];
    xml << "  <participantsessionassoc participant_id=\""
        << p.participant_id << "\" session_id=\"" << session_id_ << "\">\r\n";
    if (!assoc_time.empty())
      xml << "    <associate-time>" << assoc_time << "</associate-time>\r\n";
    xml << "    <disassociate-time>" << stop_str << "</disassociate-time>\r\n"
        << "  </participantsessionassoc>\r\n";
  }

  // Participant-Stream associations with disassociate-time
  for (size_t i = 0; i < participants_.size(); i++) {
    const SiprecParticipant& p = participants_[i];
    xml << "  <participantstreamassoc participant_id=\""
        << p.participant_id << "\">\r\n";

    for (size_t j = 0; j < streams_.size(); j++) {
      const SiprecStream& s = streams_[j];
      if (s.sender_id == p.participant_id)
        xml << "    <send>" << s.stream_id << "</send>\r\n";
      if (s.receiver_id == p.participant_id)
        xml << "    <recv>" << s.stream_id << "</recv>\r\n";
    }

    if (!assoc_time.empty())
      xml << "    <associate-time>" << assoc_time << "</associate-time>\r\n";
    xml << "    <disassociate-time>" << stop_str
        << "</disassociate-time>\r\n";
    xml << "  </participantstreamassoc>\r\n";
  }

  xml << "</recording>\r\n";
  return xml.str();
}

string SiprecMetadata::buildHoldXml(const string& held_participant_id) const {
  // RFC 7865 Section 6.8.1: on hold, send partial update removing
  // send associations for the held participant.
  std::ostringstream xml;

  xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
      << "<recording xmlns=\"" << SIPREC_METADATA_NS
      << "\" dataMode=\"partial\">\r\n";

  // Only include the affected participant-stream association
  for (size_t i = 0; i < participants_.size(); i++) {
    const SiprecParticipant& p = participants_[i];
    if (p.participant_id != held_participant_id) continue;

    xml << "  <participantstreamassoc participant_id=\""
        << p.participant_id << "\">\r\n";

    // Only recv, no send (participant is on hold)
    for (size_t j = 0; j < streams_.size(); j++) {
      const SiprecStream& s = streams_[j];
      if (s.receiver_id == p.participant_id)
        xml << "    <recv>" << s.stream_id << "</recv>\r\n";
    }
    xml << "  </participantstreamassoc>\r\n";
  }

  xml << "</recording>\r\n";
  return xml.str();
}
