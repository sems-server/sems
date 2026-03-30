/*
 * ED-137B Ground Radio Station (GRS) Simulator - SIP/SDP Helper
 *
 * Naming: "Ed137bGrs" prefix throughout this module.
 *   - ED-137B = EUROCAE standard revision B for VoIP in ATC
 *   - GRS     = Ground Radio Station (the radio equipment being simulated)
 *
 * Utilities for building and parsing ED-137B SIP headers
 * and SDP attributes per EUROCAE ED-137B specification.
 */

#ifndef _ED137B_GRS_SIP_HELPER_H_
#define _ED137B_GRS_SIP_HELPER_H_

#include "AmSdp.h"
#include <string>
#include <map>

using std::string;
using std::map;

// ED-137B SIP header names
#define ED137B_HDR_WG67_VERSION  "WG67-version"
#define ED137B_HDR_PRIORITY      "Priority"

// ED-137B SDP attribute names
#define ED137B_SDP_TYPE          "type"
#define ED137B_SDP_FREQ          "freq"
#define ED137B_SDP_TXRXMODE      "txrxmode"
#define ED137B_SDP_BSS           "bss"
#define ED137B_SDP_SQC           "sqc"
#define ED137B_SDP_CLIMAX        "climax"
#define ED137B_SDP_CLD           "cld"
#define ED137B_SDP_MID           "mid"

// R2S fmtp parameter for PCMA
#define ED137B_R2S_FMTP          "R2S"

namespace Ed137bGrsSipHelper {

  // Build ED-137B SIP headers string (WG67-version + Priority)
  string buildHeaders(const string& wg67_version, const string& priority);

  // Extract a named header value from SIP header block
  string extractHeader(const string& hdrs, const string& hdr_name);

  // Add ED-137B media-level SDP attributes to the first audio media
  void addSdpAttributes(AmSdp& sdp,
                        const string& radio_type,
                        const string& frequency,
                        const string& txrx_mode,
                        const string& channel_spacing,
                        const string& bss,
                        const string& squelch_ctrl,
                        bool climax);

  // Add R2S fmtp parameter to PCMA payload in SDP
  void addR2sFmtp(AmSdp& sdp);

  // Parse ED-137B attributes from SDP into a key-value map
  // Keys: "type", "freq", "txrxmode", "cld", "bss", "sqc", "climax"
  map<string, string> parseSdpAttributes(const AmSdp& sdp);

} // namespace Ed137bGrsSipHelper

#endif // _ED137B_GRS_SIP_HELPER_H_
