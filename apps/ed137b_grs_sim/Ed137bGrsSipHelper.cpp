/*
 * ED-137B Ground Radio Station (GRS) Simulator - SIP/SDP Helper
 */

#include "Ed137bGrsSipHelper.h"
#include "AmSipMsg.h"
#include "log.h"

namespace Ed137bGrsSipHelper {

string buildHeaders(const string& wg67_version, const string& priority)
{
  string hdrs;
  if (!wg67_version.empty())
    hdrs += string(ED137B_HDR_WG67_VERSION) + ": " + wg67_version + "\r\n";
  if (!priority.empty())
    hdrs += string(ED137B_HDR_PRIORITY) + ": " + priority + "\r\n";
  return hdrs;
}

string extractHeader(const string& hdrs, const string& hdr_name)
{
  return getHeader(hdrs, hdr_name);
}

void addSdpAttributes(AmSdp& sdp,
                      const string& radio_type,
                      const string& frequency,
                      const string& txrx_mode,
                      const string& channel_spacing,
                      const string& bss,
                      const string& squelch_ctrl,
                      bool climax)
{
  if (sdp.media.empty()) return;

  SdpMedia& media = sdp.media.front();

  if (!radio_type.empty())
    media.attributes.push_back(SdpAttribute(ED137B_SDP_TYPE, radio_type));
  if (!frequency.empty())
    media.attributes.push_back(SdpAttribute(ED137B_SDP_FREQ, frequency));
  if (!txrx_mode.empty())
    media.attributes.push_back(SdpAttribute(ED137B_SDP_TXRXMODE, txrx_mode));
  if (!channel_spacing.empty())
    media.attributes.push_back(SdpAttribute(ED137B_SDP_CLD, channel_spacing));
  if (!bss.empty())
    media.attributes.push_back(SdpAttribute(ED137B_SDP_BSS, bss));
  if (!squelch_ctrl.empty())
    media.attributes.push_back(SdpAttribute(ED137B_SDP_SQC, squelch_ctrl));
  if (climax)
    media.attributes.push_back(SdpAttribute(ED137B_SDP_CLIMAX));
}

void addR2sFmtp(AmSdp& sdp)
{
  if (sdp.media.empty()) return;

  SdpMedia& media = sdp.media.front();
  for (auto& pl : media.payloads) {
    if (pl.payload_type == 8 || pl.encoding_name == "PCMA") {
      pl.sdp_format_parameters = ED137B_R2S_FMTP;
      DBG("ED137B-GRS: set R2S fmtp on PCMA payload\n");
      break;
    }
  }
}

map<string, string> parseSdpAttributes(const AmSdp& sdp)
{
  map<string, string> result;
  if (sdp.media.empty()) return result;

  const SdpMedia& media = sdp.media.front();
  for (const auto& attr : media.attributes) {
    if (attr.attribute == ED137B_SDP_TYPE ||
        attr.attribute == ED137B_SDP_FREQ ||
        attr.attribute == ED137B_SDP_TXRXMODE ||
        attr.attribute == ED137B_SDP_CLD ||
        attr.attribute == ED137B_SDP_BSS ||
        attr.attribute == ED137B_SDP_SQC ||
        attr.attribute == ED137B_SDP_MID) {
      result[attr.attribute] = attr.value;
    }
    else if (attr.attribute == ED137B_SDP_CLIMAX) {
      result[ED137B_SDP_CLIMAX] = "true";
    }
  }
  return result;
}

} // namespace Ed137bGrsSipHelper
