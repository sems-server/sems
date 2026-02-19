/*
 * Minimal SIPREC SRS (Session Recording Server) for SEMS.
 */

#include "SiprecSrs.h"
#include "AmSdp.h"
#include "AmMimeBody.h"
#include "AmUtils.h"
#include "AmConfig.h"
#include "log.h"

#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#define MOD_NAME "siprec_srs"
#define SIPREC_METADATA_CT "application/rs-metadata+xml"

EXPORT_SESSION_FACTORY(SiprecSrsFactory, MOD_NAME);

// --- Factory ---

string SiprecSrsFactory::recording_dir_;

SiprecSrsFactory::SiprecSrsFactory(const string& name)
  : AmSessionFactory(name)
{
}

int SiprecSrsFactory::onLoad() {
  AmConfigReader cfg;
  if (cfg.loadPluginConf(MOD_NAME)) {
    WARN("SRS: no configuration file found, using defaults\n");
  }

  recording_dir_ = cfg.getParameter("recording_dir", "/var/spool/sems/siprec");

  unsigned short port_min = 40000, port_max = 40999;
  string s;
  s = cfg.getParameter("rtp_port_min", "40000");
  if (!s.empty()) port_min = (unsigned short)atoi(s.c_str());
  s = cfg.getParameter("rtp_port_max", "40999");
  if (!s.empty()) port_max = (unsigned short)atoi(s.c_str());

  g_port_allocator.configure(port_min, port_max);

  // Ensure recording directory exists and is writable
  if (mkdir(recording_dir_.c_str(), 0755) != 0 && errno != EEXIST) {
    ERROR("SRS: cannot create recording_dir '%s': %s\n",
          recording_dir_.c_str(), strerror(errno));
    return -1;
  }

  struct stat st;
  if (stat(recording_dir_.c_str(), &st) != 0) {
    ERROR("SRS: recording_dir '%s' does not exist\n", recording_dir_.c_str());
    return -1;
  }
  if (!S_ISDIR(st.st_mode)) {
    ERROR("SRS: recording_dir '%s' is not a directory\n", recording_dir_.c_str());
    return -1;
  }
  if (access(recording_dir_.c_str(), W_OK) != 0) {
    ERROR("SRS: recording_dir '%s' is not writable\n", recording_dir_.c_str());
    return -1;
  }

  INFO("SRS: loaded (recording_dir='%s', ports=%u-%u)\n",
       recording_dir_.c_str(), port_min, port_max);

  return 0;
}

AmSession* SiprecSrsFactory::onInvite(const AmSipRequest& req,
                                       const string& app_name,
                                       const map<string, string>& app_params) {
  // Verify this is a SIPREC INVITE
  string require = getHeader(req.hdrs, "Require", true);
  if (require.find("siprec") == string::npos) {
    WARN("SRS: INVITE without Require: siprec header, rejecting\n");
    throw AmSession::Exception(400, "Missing Require: siprec");
  }

  return new SiprecSrsSession();
}

// --- Session ---

SiprecSrsSession::SiprecSrsSession()
  : codec_pt_(8)
{
  // Generate unique session ID for file naming
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  char buf[64];
  snprintf(buf, sizeof(buf), "%ld_%09ld", ts.tv_sec, ts.tv_nsec);
  session_id_ = buf;
  recording_dir_ = SiprecSrsFactory::getRecordingDir();
}

SiprecSrsSession::~SiprecSrsSession() {
  receiver_a_.shutdown();
  receiver_b_.shutdown();
}

void SiprecSrsSession::onInvite(const AmSipRequest& req) {
  // Parse and save metadata before sending 200 OK
  parseInvite(req);

  // Let the base class handle SDP answer + 200 OK
  // getSdpAnswer() will be called automatically by dlg->reply()
  dlg->reply(req, 200, "OK");
}

bool SiprecSrsSession::parseInvite(const AmSipRequest& req) {
  // Extract SIPREC metadata XML from multipart body
  const AmMimeBody* xml_part = req.body.hasContentType(SIPREC_METADATA_CT);
  if (xml_part && xml_part->getPayload()) {
    string xml((const char*)xml_part->getPayload(), xml_part->getLen());
    saveMetadata(xml, "initial");
    INFO("SRS: saved initial metadata for session %s\n", session_id_.c_str());
  } else {
    WARN("SRS: no SIPREC metadata in INVITE\n");
  }

  return true;
}

void SiprecSrsSession::saveMetadata(const string& xml, const string& suffix) {
  string path = recording_dir_ + "/" + session_id_ + "_" + suffix + ".xml";
  FILE* fp = fopen(path.c_str(), "w");
  if (fp) {
    fwrite(xml.c_str(), 1, xml.length(), fp);
    fclose(fp);
  } else {
    ERROR("SRS: cannot write metadata to '%s'\n", path.c_str());
  }
}

bool SiprecSrsSession::getSdpAnswer(const AmSdp& offer, AmSdp& answer) {
  // Build SDP answer with 2 recvonly m-lines

  DBG("SRS: getSdpAnswer - offer has %zu m-lines\n", offer.media.size());
  for (size_t i = 0; i < offer.media.size(); i++) {
    DBG("SRS:   m-line %zu: type=%d transport=%d port=%u send=%d recv=%d payloads=%zu\n",
        i, offer.media[i].type, offer.media[i].transport,
        offer.media[i].port, offer.media[i].send, offer.media[i].recv,
        offer.media[i].payloads.size());
  }

  answer.version = 0;
  answer.origin.user = "sems-srs";
  answer.origin.sessId = (unsigned int)time(NULL);
  answer.origin.sessV = 1;
  answer.sessionName = "SIPREC SRS";
  // derive address type from offer (follow the caller's address family)
  int addr_type = offer.conn.address.empty() ? AT_V4 : offer.conn.addrType;
  answer.conn.network = NT_IN;
  answer.conn.addrType = addr_type;
  answer.conn.address = advertisedIP(addr_type);
  answer.media.clear();

  string local_ip = localMediaIP(addr_type);
  int leg = 0;

  for (vector<SdpMedia>::const_iterator m_it = offer.media.begin();
       m_it != offer.media.end(); ++m_it) {

    answer.media.push_back(SdpMedia());
    SdpMedia& am = answer.media.back();

    if (m_it->type != MT_AUDIO || m_it->transport != TP_RTPAVP) {
      // Reject non-audio or disabled m-lines
      am.type = m_it->type;
      am.transport = m_it->transport;
      am.port = 0;
      am.nports = 0;
      am.send = false;
      am.recv = false;
      if (!m_it->payloads.empty())
        am.payloads.push_back(m_it->payloads.front());
      continue;
    }

    // Determine codec from offer
    int pt = 8;  // default PCMA
    string codec_name = "PCMA";
    int clock_rate = 8000;
    for (vector<SdpPayload>::const_iterator p = m_it->payloads.begin();
         p != m_it->payloads.end(); ++p) {
      // Accept PCMA (8) or PCMU (0)
      if (p->payload_type == 8 || p->payload_type == 0) {
        pt = p->payload_type;
        codec_name = (pt == 0) ? "PCMU" : "PCMA";
        clock_rate = 8000;
        break;
      }
    }

    if (leg == 0) codec_pt_ = pt;  // remember for both legs

    // Allocate RTP port
    unsigned short rtp_port = g_port_allocator.allocate();

    // Set up receiver for this leg
    string wav_path = recording_dir_ + "/" + session_id_ +
                      (leg == 0 ? "_leg1.wav" : "_leg2.wav");
    RtpReceiver& rcv = (leg == 0) ? receiver_a_ : receiver_b_;

    unsigned short bound_port = rcv.init(local_ip, rtp_port, wav_path, pt);
    if (bound_port == 0) {
      ERROR("SRS: failed to bind RTP port %u for leg %d\n", rtp_port, leg + 1);
      am.type = MT_AUDIO;
      am.transport = TP_RTPAVP;
      am.port = 0;
      am.send = false;
      am.recv = false;
      if (!m_it->payloads.empty())
        am.payloads.push_back(m_it->payloads.front());
      leg++;
      continue;
    }

    // Build answer m-line: recvonly
    am.type = MT_AUDIO;
    am.transport = TP_RTPAVP;
    am.port = bound_port;
    am.nports = 0;
    am.send = false;   // recvonly
    am.recv = true;

    // Single payload matching the offer
    am.payloads.push_back(SdpPayload(pt, codec_name, clock_rate, -1));

    // Copy label attribute if present
    for (vector<SdpAttribute>::const_iterator a = m_it->attributes.begin();
         a != m_it->attributes.end(); ++a) {
      if (a->attribute == "label") {
        am.attributes.push_back(*a);
        break;
      }
    }

    // Start receiver thread
    rcv.start();

    INFO("SRS: leg %d - port %u, codec %s (pt=%d), file '%s'\n",
         leg + 1, bound_port, codec_name.c_str(), pt, wav_path.c_str());

    leg++;
    if (leg >= 2) break;  // only accept first 2 audio m-lines
  }

  if (leg == 0) {
    ERROR("SRS: no audio m-lines in offer\n");
    return false;
  }

  INFO("SRS: recording session %s started (%d legs)\n",
       session_id_.c_str(), leg);

  return true;
}

int SiprecSrsSession::onSdpCompleted(const AmSdp& offer, const AmSdp& answer) {
  // Don't set up AmSession's built-in RTP — we handle RTP ourselves
  return 0;
}

void SiprecSrsSession::onSessionStart() {
  // No media processing via AmSession — RTP receivers handle everything
  DBG("SRS: session %s started\n", session_id_.c_str());
}

void SiprecSrsSession::onBye(const AmSipRequest& req) {
  INFO("SRS: BYE received for session %s\n", session_id_.c_str());

  // Save final metadata if present in BYE body
  const AmMimeBody* xml_part = req.body.hasContentType(SIPREC_METADATA_CT);
  if (!xml_part) {
    // Try multipart
    xml_part = req.body.hasContentType("multipart/mixed");
    if (xml_part) {
      xml_part = xml_part->hasContentType(SIPREC_METADATA_CT);
    }
  }

  if (xml_part && xml_part->getPayload()) {
    string xml((const char*)xml_part->getPayload(), xml_part->getLen());
    saveMetadata(xml, "final");
    INFO("SRS: saved final metadata for session %s\n", session_id_.c_str());
  }

  // Stop receivers and finalize WAV files
  receiver_a_.shutdown();
  receiver_b_.shutdown();

  INFO("SRS: recording session %s complete\n", session_id_.c_str());

  setStopped();
}
