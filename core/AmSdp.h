/*
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. This program is released under
 * the GPL with the additional exemption that compiling, linking,
 * and/or using OpenSSL is allowed.
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

#ifndef __SdpParser__
#define __SdpParser__

#include <string>
#include <map>
#include <vector>
#include <netinet/in.h>
#include "AmPlugIn.h"
#include <memory>
using std::string;


#define COMFORT_NOISE_PAYLOAD_TYPE 13 // RFC 3389
#define DYNAMIC_PAYLOAD_TYPE_START 96 // range: 96->127, see RFC 1890
/**
 * @file AmSdp.h
 * Definitions for the SDP parser.
 * Refer to the SDP RFC document for any further information.
 */

/** Scratch buffer size. */
#define BUFFER_SIZE 4096

/** network type */
enum NetworkType { NT_OTHER=0, NT_IN };
/** address type */
enum AddressType { AT_NONE=0, AT_V4, AT_V6 }; 
/** media type */
enum MediaType { MT_NONE=0, MT_AUDIO, MT_VIDEO, MT_APPLICATION, MT_TEXT, MT_MESSAGE, MT_IMAGE };
/** transport protocol */
enum TransProt { TP_NONE=0, TP_RTPAVP, TP_UDP, TP_RTPSAVP, TP_UDPTL, TP_RTPSAVPF };

/** \brief c=... line in SDP*/
struct SdpConnection
{
  /** @see NetworkType */
  int network;
  /** @see AddressType */
  int addrType;

  struct sockaddr_in ipv4;
  struct sockaddr_in6 ipv6;
  /** IP address */
  string address;

  SdpConnection() : address() {}

  bool operator == (const SdpConnection& other) const;
  /** pretty print */
  string debugPrint() const;
};

/** \brief o=... line in SDP */
struct SdpOrigin
{
  string user;
  unsigned int sessId;
  unsigned int sessV;
  SdpConnection conn;

  SdpOrigin() : user(), conn() {}

  SdpOrigin(const SdpOrigin& other)
    : user(other.user), sessId(other.sessId), sessV(other.sessV),
      conn(other.conn)
  {}

  bool operator == (const SdpOrigin& other) const;
};
/** 
 * \brief sdp payload
 *
 * this binds together pt, name, rate and parameters
 */
struct SdpPayload
{
  int    type;   // media type
  int    payload_type; // SDP payload type
  string encoding_name;
  int    clock_rate; // sample rate (Hz)
  string format;
  string sdp_format_parameters;
  int    encoding_param;
  
  SdpPayload() 
    : payload_type(-1), 
      clock_rate(-1), 
      encoding_param(-1) 
  {}

  SdpPayload(int pt) 
    : payload_type(pt), 
      clock_rate(-1), 
      encoding_param(-1) 
  {}

  SdpPayload(int pt, const string& name, int rate, int param) 
    : payload_type(pt), encoding_name(name), 
      clock_rate(rate), encoding_param(param) 
  {}

  SdpPayload(const SdpPayload& other)
    : type(other.type), payload_type(other.payload_type),
      encoding_name(other.encoding_name), clock_rate(other.clock_rate),
      format(other.format), sdp_format_parameters(other.sdp_format_parameters),
      encoding_param(other.encoding_param)
  {}

  bool operator == (int r);

  bool operator == (const SdpPayload& other) const;
};

/** \brief a=... line in SDP */
struct SdpAttribute
{
  string attribute;
  string value;

  // property attribute
  SdpAttribute(const string& attribute,
	       const string& value)
    : attribute(attribute), value(value) { }

  // value attribute
  SdpAttribute(const string& attribute)
    : attribute(attribute) { }

  SdpAttribute(const SdpAttribute& other)
    : attribute(other.attribute), value(other.value) {}

  string print() const;

  bool operator == (const SdpAttribute& other) const;
};

/** \brief m=... line in SDP */
struct SdpMedia
{
  enum Direction {
    DirBoth=0,
    DirActive=1,
    DirPassive=2,
    DirUndefined=3
  };

  int           type;
  unsigned int  port;
  unsigned int  nports;
  int           transport;
  SdpConnection conn; // c=
  Direction     dir;  // a=direction
  string        fmt;  // format in case proto != RTP/AVP or RTP/SAVP

  // sendrecv|sendonly|recvonly|inactive
  bool          send;
  bool          recv;

  std::vector<SdpPayload> payloads;

  std::vector<SdpAttribute> attributes; // unknown attributes

  bool operator == (const SdpMedia& other) const;

  SdpMedia() : conn(), dir(DirUndefined), type(MT_NONE), transport(TP_NONE), send(true), recv(true) {}

  /** pretty print */
  string debugPrint() const;

  static string type2str(int type);

  /**
   * Checks which payloads are compatible with the payload provider,
   * inserts them into the answer, compute send/recv attributes
   * and direction according to the offer.
   */
  void calcAnswer(const AmPayloadProvider* payload_prov, 
		  SdpMedia& answer) const;
};

/**
 * \brief handling SDP attribute "rtcp"
 */
class RtcpAddress
{
  private:
    string nettype, addrtype, address;
    bool parse(const string &src);
    int port;

  public:
    RtcpAddress(const string &attr_value);
    bool hasAddress() { return !address.empty(); }
    void setAddress(const string &addr) { address = addr; }
    const string& getAddress() { return address; }
    void setPort(int _port) { port = _port; }
    int getPort() { return port; }
    string print();
};

/**
 * \brief The SDP parser class.
 */
class AmSdp
{

  /**
   * Find payload by name, return cloned object
   */
  const SdpPayload *findPayload(const string& name) const;

public:
  // parsed SDP definition
  unsigned int     version;     // v=
  SdpOrigin        origin;      // o=
  string           sessionName; // s= 
  string           uri;         // u=
  SdpConnection    conn;        // c=
  std::vector<SdpAttribute> attributes; // unknown session level attributes

  std::vector<SdpMedia> media;  // m= ... [a=rtpmap:...]+
  //TODO: t= lines

  SdpOrigin        l_origin;      // local origin (o= )
    
  AmSdp();
  AmSdp(const AmSdp& p_sdp_msg);

  /** 
   * Parse the SDP message passed as an argument.
   * @return !=0 if error encountered.
   */
  int parse(const char* _sdp_msg);

  /**
   * Prints the current SDP structure
   * into a proper SDP message.
   */
  void print(string& body) const;

  /** get telephone event payload */
  const SdpPayload *telephoneEventPayload() const;

  /**
   * Test if remote UA supports 'telefone_event'.
   */
  //bool hasTelephoneEvent();

  bool operator == (const AmSdp& other) const;

  /**
   * Clear all parsed values.
   */
  void clear();
};

#endif

// Local Variables:
// mode:C++
// End:
