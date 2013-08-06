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

int filterSDP(AmSdp& sdp, const vector<FilterEntry>& filter_list) {
  
  for (vector<FilterEntry>::const_iterator it=
	 filter_list.begin(); it!=filter_list.end(); it++){

    const FilterType& sdpfilter = it->filter_type;
    const std::set<string>& sdpfilter_list = it->filter_list;

    bool media_line_filtered_out = false;
    bool media_line_left = false;

    if (!isActiveFilter(sdpfilter))
      continue;

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
      // if no payload supported any more: leave at least one, and set port to 0
      // RFC 3264 section 6.1
      if(!new_pl.size() && media.payloads.size()) {
	std::vector<SdpPayload>::iterator p_it = media.payloads.begin();
	new_pl.push_back(*p_it);
	media.port = 0;
        media_line_filtered_out = true;
      }
      else media_line_left = true;
      media.payloads = new_pl;
    }
    if ((!media_line_left) && media_line_filtered_out) {
      // no filter adds new payloads, we can safely return error
      DBG("all streams were marked as inactive\n");
      return -488;
    }
  }

  return 0;
}

int filterMedia(AmSdp& sdp, const vector<FilterEntry>& filter_list)
{
  unsigned filtered_out = 0;

  DBG("filtering media types\n");
  for (vector<FilterEntry>::const_iterator i = filter_list.begin(); i !=filter_list.end(); ++i) {

    const FilterType& filter = i->filter_type;
    const std::set<string>& media_list = i->filter_list;

    if (!isActiveFilter(filter)) continue;

    for (std::vector<SdpMedia>::iterator m = sdp.media.begin(); m != sdp.media.end(); ++m) {
      if (m->port == 0) continue; // already inactive

      string type(SdpMedia::type2str(m->type));
      DBG("checking whether to filter out '%s'\n", type.c_str());

      bool is_filtered = (filter == Whitelist) ^ (media_list.find(type) != media_list.end());
      if (is_filtered) {
        m->port = 0;
        filtered_out++;
      }
    }
  }

  if (filtered_out > 0 && filtered_out == sdp.media.size()) {
    // we filtered out all media lines (if there was an inactive stream before
    // it was probably intendend)
    DBG("all streams were marked as inactive\n");
    return -488;
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

std::vector<SdpAttribute> filterSDPAttributes(std::vector<SdpAttribute> attributes,
  FilterType sdpalinesfilter, const std::set<string>& sdpalinesfilter_list) {

  std::vector<SdpAttribute> res;
  for (std::vector<SdpAttribute>::iterator a_it =
    attributes.begin(); a_it != attributes.end(); a_it++) {
    
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
	res.push_back(*a_it);
  }
  return res;
}

int filterSDPalines(AmSdp& sdp, const vector<FilterEntry>& filter_list) {
  for (vector<FilterEntry>::const_iterator it=
	 filter_list.begin(); it!=filter_list.end(); it++){

    const FilterType& sdpalinesfilter = it->filter_type;
    const std::set<string>& sdpalinesfilter_list = it->filter_list;

    // If not Black- or Whitelist, simply return
    if (!isActiveFilter(sdpalinesfilter))
      continue;
  
    // We start with per Session-alines
    sdp.attributes =
      filterSDPAttributes(sdp.attributes, sdpalinesfilter, sdpalinesfilter_list);

    for (std::vector<SdpMedia>::iterator m_it =
	   sdp.media.begin(); m_it != sdp.media.end(); m_it++) {
      SdpMedia& media = *m_it;
      // todo: what if no payload supported any more?
      media.attributes =
	filterSDPAttributes(media.attributes, sdpalinesfilter, sdpalinesfilter_list);
    }
  }

  return 0;
}

int normalizeSDP(AmSdp& sdp, bool anonymize_sdp, const string &advertised_ip) {
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
    // Clear s-Line in SDP:
    sdp.sessionName = "-";
    // Clear u-Line in SDP:
    sdp.uri.clear();
    // Clear origin user
    sdp.origin.user = "-";
    if (!advertised_ip.empty()) {
      sdp.origin.conn.address = advertised_ip;
      // SdpConnection::ipv4/ipv6 seems not to be used so we won't replace it there
      // TODO: replace in attributes
    }
  }

  return 0;
}


