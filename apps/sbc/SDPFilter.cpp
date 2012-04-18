/*
 * Copyright (C) 2010 Stefan Sayer
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For a license to use the SEMS software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * SEMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "SDPFilter.h"
#include <algorithm>
#include "log.h"
#include "AmUtils.h"
#include "RTPParameters.h"

int filterSDP(AmSdp& sdp, FilterType sdpfilter, const std::set<string>& sdpfilter_list) {

  if (!isActiveFilter(sdpfilter))
    return 0;

  for (std::vector<SdpMedia>::iterator m_it =
	 sdp.media.begin(); m_it != sdp.media.end(); m_it++) {
    SdpMedia& media = *m_it;

    std::vector<SdpPayload> new_pl;
    for (std::vector<SdpPayload>::iterator p_it =
	   media.payloads.begin(); p_it != media.payloads.end(); p_it++) {
      
      string c = p_it->encoding_name;
      std::transform(c.begin(), c.end(), c.begin(), ::tolower);
      
      bool is_filtered =  (sdpfilter == Whitelist) ^
	(sdpfilter_list.find(c) != sdpfilter_list.end());

      // DBG("%s (%s) is_filtered: %s\n", p_it->encoding_name.c_str(), c.c_str(), 
      // 	  is_filtered?"true":"false");

      if (!is_filtered)
	new_pl.push_back(*p_it);
    }
    if (new_pl.empty()) {
      // in case of SDP offer we could remove media line but in case of answer
      // we should just reject the stream by setting port to 0 (at least one
      // format must be given; see RFC 3264, sect. 6)
      media.port = 0;
      if (media.payloads.size() > 1) 
        media.payloads.erase(media.payloads.begin() + 1, media.payloads.end());
    }
    else media.payloads = new_pl;    
  }

  return 0;
}

void fix_missing_encodings(SdpMedia& m) {
  for (std::vector<SdpPayload>::iterator p_it=
	 m.payloads.begin(); p_it!=m.payloads.end(); p_it++) {
    SdpPayload& p = *p_it;
    if (!p.encoding_name.empty())
      continue;
    if (p.payload_type > (IANA_RTP_PAYLOADS_SIZE-1) || p.payload_type < 0)
      continue; // todo: throw out this payload
    if (IANA_RTP_PAYLOADS[p.payload_type].payload_name[0]=='\0')
      continue; // todo: throw out this payload

    p.encoding_name = IANA_RTP_PAYLOADS[p.payload_type].payload_name;
    p.clock_rate = IANA_RTP_PAYLOADS[p.payload_type].clock_rate;
    if (IANA_RTP_PAYLOADS[p.payload_type].channels > 1)
      p.encoding_param = IANA_RTP_PAYLOADS[p.payload_type].channels;

    DBG("named SDP payload type %d with %s/%d%s\n",
	p.payload_type, IANA_RTP_PAYLOADS[p.payload_type].payload_name,
	IANA_RTP_PAYLOADS[p.payload_type].clock_rate,
	IANA_RTP_PAYLOADS[p.payload_type].channels > 1 ?
	("/"+int2str(IANA_RTP_PAYLOADS[p.payload_type].channels)).c_str() : "");
  }
}

void fix_incomplete_silencesupp(SdpMedia& m) {
  for (std::vector<SdpAttribute>::iterator a_it =
	 m.attributes.begin(); a_it != m.attributes.end(); a_it++) {
    if (a_it->attribute == "silenceSupp") {
      vector<string> parts = explode(a_it->value, " ");
      if (parts.size() < 5) {
	string val_before = a_it->value;
	for (int i=parts.size();i<5;i++)
	  a_it->value += " -";
	DBG("fixed SDP attribute silenceSupp:'%s' -> '%s'\n",
	    val_before.c_str(), a_it->value.c_str());
      }
    }
  }
}

std::vector<SdpAttribute> filterAlinesInternal(std::vector<SdpAttribute> list, 
  FilterType sdpalinesfilter, const std::set<string>& sdpalinesfilter_list) {

  std::vector<SdpAttribute> new_alines;
  for (std::vector<SdpAttribute>::iterator a_it =
    list.begin(); a_it != list.end(); a_it++) {
    
    // Case insensitive search:
    string c = a_it->attribute;
    std::transform(c.begin(), c.end(), c.begin(), ::tolower);
    
    // Check, if this should be filtered:
    bool is_filtered =  (sdpalinesfilter == Whitelist) ^
      (sdpalinesfilter_list.find(c) != sdpalinesfilter_list.end());

    DBG("%s (%s) is_filtered: %s\n", a_it->attribute.c_str(), c.c_str(), 
     	  is_filtered?"true":"false");
 
    // If it is not filtered, just add it to the list:
    if (!is_filtered)
	new_alines.push_back(*a_it);
  }
  return new_alines;
}

int filterSDPalines(AmSdp& sdp, FilterType sdpalinesfilter,
		    const std::set<string>& sdpalinesfilter_list) {
  // If not Black- or Whitelist, simply return
  if (!isActiveFilter(sdpalinesfilter))
    return 0;
  
  // We start with per Session-alines
  sdp.attributes =
    filterAlinesInternal(sdp.attributes, sdpalinesfilter, sdpalinesfilter_list);

  for (std::vector<SdpMedia>::iterator m_it =
	 sdp.media.begin(); m_it != sdp.media.end(); m_it++) {
    SdpMedia& media = *m_it;
    // todo: what if no payload supported any more?
    media.attributes =
      filterAlinesInternal(media.attributes, sdpalinesfilter, sdpalinesfilter_list);
  }

  return 0;
}

int normalizeSDP(AmSdp& sdp, bool anonymize_sdp) {
  for (std::vector<SdpMedia>::iterator m_it=
	 sdp.media.begin(); m_it != sdp.media.end(); m_it++) {
    if (m_it->type != MT_AUDIO && m_it->type != MT_VIDEO)
      continue;

    // fill missing encoding names (a= lines)
    fix_missing_encodings(*m_it);

    // fix incomplete silenceSupp attributes (see RFC3108)
    // (only media level - RFC3108 4.)
    fix_incomplete_silencesupp(*m_it);
  }

  if (anonymize_sdp) {
    // Clear s-Line with call:
    sdp.sessionName = "call";
    // Clear u-Line in SDP:
    sdp.uri.clear();
    // Clear origin user
    sdp.origin.user = "-";
  }


  return 0;
}
