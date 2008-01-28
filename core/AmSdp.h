/*
 * $Id$
 *
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of sems, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * sems is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __SdpParser__
#define __SdpParser__

#include <string>
#include <map>
#include <vector>
using std::string;


#define COMFORT_NOISE_PAYLOAD_TYPE 13 // RFC 3389
#define DYNAMIC_PAYLOAD_TYPE_START 96 // range: 96->127, see RFC 1890
/**
 * @file AmSdp.h
 * Definitions for the SDP parser.
 * Refer to the SDP RFC document for any further information.
 */

/** Scratch buffer size. */
#define BUFFER_SIZE 2048

/** network type */
enum NetworkType { NT_OTHER=0, NT_IN };
/** address type */
enum AddressType { AT_NONE=0, AT_V4, AT_V6 }; 
/** media type */
enum MediaType { MT_NONE=0, MT_AUDIO, MT_VIDEO, MT_APPLICATION, MT_DATA };
/** transport protocol */
enum TransProt { TP_NONE=0, TP_RTPAVP, TP_UDP };

/** \brief c=... line in SDP*/
struct SdpConnection
{
  /** @see NetworkType */
  int network;
  /** @see AddressType */
  int addrType;
  /** IP address */
  string address;

  SdpConnection() : address() {}
};

/** \brief o=... line in SDP */
struct SdpOrigin
{
  string user;
  unsigned int sessId;
  unsigned int sessV;
  SdpConnection conn;
};
/** 
 * \brief sdp payload
 *
 * this binds together pt, name, rate and parameters
 */
struct SdpPayload
{
  int    int_pt; // internal payload type
  int    payload_type; // SDP payload type
  string encoding_name;
  int    clock_rate; // sample rate (Hz)
  string sdp_format_parameters;
  
  SdpPayload() : int_pt(-1), payload_type(-1), clock_rate(-1) {}

  SdpPayload(int pt) : int_pt(-1), payload_type(pt), clock_rate(-1) {}

  SdpPayload(int pt, const string& name, int rate) 
    : int_pt(-1), payload_type(pt), encoding_name(name), clock_rate(rate) {}

  bool operator == (int r);
};


/** \brief m=... line in SDP */
struct SdpMedia
{
  enum Direction {
    DirBoth=0,
    DirActive=1,
    DirPassive=2
  };

  int           type;
  unsigned int  port;
  int           transport;
  SdpConnection conn; // c=
  Direction     dir;  // a=direction

  std::vector<SdpPayload> payloads;

  SdpMedia() : conn() {}
};

/**
 * \brief The SDP parser class.
 */
class AmSdp
{
  /** scratch buffer */
  char r_buf[BUFFER_SIZE];

  // Remote payload type for 
  // 'telephone-event'
  const SdpPayload *telephone_event_pt;

  /** 
   * Do we have that payload ? 
   * @return our payload type.
   */
  int getDynPayload(const string& name, int rate);

  /**
   * Find payload by name
   */
  const SdpPayload *findPayload(const string& name);

public:
  // parsed SDP definition
  unsigned int     version;     // v=
  SdpOrigin        origin;      // o=
  string           sessionName; // s= 
  string           uri;         // u=
  SdpConnection    conn;        // c=
  std::vector<SdpMedia> media;  // m= ... [a=rtpmap:...]+

  // Supported payloads
  std::vector<SdpPayload*> sup_pl; 
  // Is remote host requesting 
  // us to do passive RTP ?
  bool remote_active;
    
  AmSdp();
  AmSdp(const AmSdp& p_sdp_msg);

  /** Sets the SDP offer to be parsed. */
  void setBody(const char* _sdp_msg);
  /** Parse the invitation 
   * @return !=0 if error encountered.
   */
  int parse();

  /** 
   * Generate an SDP answer to the offer parsed previously. 
   * @return !=0 if error encountered.
   */
  int genResponse(const string& localip, int localport, 
		  string& out_buf, bool single_codec = false);

  /** 
   * Generate an SDP offer. 
   * @return !=0 if error encountered.
   */
  int genRequest(const string& localip,int localport, string& out_buf);

  /** 
   * Get a compatible payload from SDP offer/response. 
   * @return empty vector if error encountered.
   */
  const std::vector<SdpPayload*>& getCompatiblePayloads(int media_type, string& addr, int& port);

  /**
   * Test if remote UA supports 'telefone_event'.
   */
  bool hasTelephoneEvent();

  const SdpPayload *telephoneEventPayload() const { return telephone_event_pt; }
};

#endif

// Local Variables:
// mode:C++
// End:
