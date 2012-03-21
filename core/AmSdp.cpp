/*
 *parse or be parsed
 */


#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "AmConfig.h"
#include "AmSdp.h"
#include "AmUtils.h"
#include "AmPlugIn.h"
#include "AmSession.h"

#include "amci/amci.h"
#include "log.h"

#include <string>
#include <map>
using std::string;
using std::map;

// Not on Solaris!
#if !defined (__SVR4) && !defined (__sun)
#include "strings.h"
#endif

static void parse_session_attr(AmSdp* sdp_msg, char* s, char** next);
static bool parse_sdp_line_ex(AmSdp* sdp_msg, char*& s);
static void parse_sdp_connection(AmSdp* sdp_msg, char* s, char t);
static void parse_sdp_media(AmSdp* sdp_msg, char* s);
static void parse_sdp_attr(AmSdp* sdp_msg, char* s);
static void parse_sdp_origin(AmSdp* sdp_masg, char* s);

inline char* get_next_line(char* s);
static char* is_eql_next(char* s);
static char* parse_until(char* s, char end);
static char* parse_until(char* s, char* end, char c);
static bool contains(char* s, char* next_line, char c);
static bool is_wsp(char s);

static int media_type(std::string media);
static int transport_type(std::string transport);
static bool attr_check(std::string attr);

enum parse_st {SDP_DESCR, SDP_MEDIA};
enum sdp_connection_st {NET_TYPE, ADDR_TYPE, IP4, IP6};
enum sdp_media_st {MEDIA, PORT, PROTO, FMT}; 
enum sdp_attr_rtpmap_st {TYPE, ENC_NAME, CLK_RATE, ENC_PARAM};
enum sdp_attr_fmtp_st {FORMAT, FORMAT_PARAM};
enum sdp_origin_st {USER, ID, VERSION_ST, NETTYPE, ADDR, UNICAST_ADDR};

// inline functions

inline string net_t_2_str(int nt)
{
  switch(nt){
  case NT_IN: return "IN";
  default: return "<unknown network type>";
  }
}

inline string addr_t_2_str(int at)
{
  switch(at){
  case AT_V4: return "IP4";
  case AT_V6: return "IP6";
  default: return "<unknown address type>";
  }
}


inline string media_t_2_str(int mt)
{
  switch(mt){
  case MT_AUDIO: return "audio";
  case MT_VIDEO: return "video";
  case MT_APPLICATION: return "application";
  case MT_TEXT: return "text";
  case MT_MESSAGE: return "message";
  default: return "<unknown media type>";
  }
}

inline string transport_p_2_str(int tp)
{
  switch(tp){
  case TP_RTPAVP: return "RTP/AVP";
  case TP_UDP: return "udp";
  case TP_RTPSAVP: return "RTP/SAVP";
  default: return "<unknown media type>";
  }
}

bool SdpConnection::operator == (const SdpConnection& other) const
{
  return network == other.network && addrType == other.addrType 
    && address == other.address;
}

bool SdpOrigin::operator == (const SdpOrigin& other) const
{
  return user == other.user && sessId == other.sessId 
    && sessV == other.sessV && conn == other.conn;
}

bool SdpPayload::operator == (const SdpPayload& other) const
{
  return payload_type == other.payload_type && encoding_name == other.encoding_name 
    && clock_rate == other.clock_rate && encoding_param == other.encoding_param;
}

string SdpConnection::debugPrint() const {
  return addr_t_2_str(addrType) + " " + address;
}

string SdpMedia::debugPrint() const {
  string payload_list;
  for(std::vector<SdpPayload>::const_iterator it=
	payloads.begin(); it!= payloads.end(); it++) {
    if (it != payloads.begin())
      payload_list+=" ";
    payload_list+=int2str(it->payload_type);
  }
  return "port "+int2str(port) + ", payloads: "+payload_list;
}

bool SdpPayload::operator == (int r)
{
  DBG("pl == r: payload_type = %i; r = %i\n", payload_type, r);
  return payload_type == r;
}

string SdpAttribute::print() const
{
  if (value.empty())
    return "a="+attribute+CRLF;
  else
    return "a="+attribute+":"+value+CRLF;
}

bool SdpAttribute::operator==(const SdpAttribute& other) const
{
  return attribute==other.attribute && (value.empty() || (value==other.value));
}

bool SdpMedia::operator == (const SdpMedia& other) const
{
  if (payloads.empty()) {
    if (!other.payloads.empty())
      return false;

  } else if (other.payloads.empty()) {
    return false;

  } else {    
    std::pair<vector<SdpPayload>::const_iterator, vector<SdpPayload>::const_iterator> pl_mismatch 
      = std::mismatch(payloads.begin(), payloads.end(), other.payloads.begin());

    if (pl_mismatch.first != payloads.end() || pl_mismatch.second != other.payloads.end())
      return false;
  }

  if (attributes.empty()) {
    if (!other.attributes.empty()) {
      return false;
    }
  } else {
    std::pair<vector<SdpAttribute>::const_iterator, vector<SdpAttribute>::const_iterator> a_mismatch 
      = std::mismatch(attributes.begin(), attributes.end(), other.attributes.begin());

    if (a_mismatch.first != attributes.end() || a_mismatch.second != other.attributes.end())
      return false;
  }

  return type == other.type && port == other.port && nports == other.nports 
    && transport == other.transport && conn == other.conn && dir == other.dir
    && send == other.send && recv == other.recv;
}

//
// class AmSdp: Methods
//
AmSdp::AmSdp()
  : origin(),
    sessionName(),
    conn(),
    media(),
    version(0)
{
  origin.user = "sems";
  origin.sessId = get_random();
  origin.sessV = get_random();
}

AmSdp::AmSdp(const AmSdp& p_sdp_msg)
  : version(p_sdp_msg.version),
    origin(p_sdp_msg.origin),
    sessionName(p_sdp_msg.sessionName),
    conn(p_sdp_msg.conn),
    media(p_sdp_msg.media),
    attributes(p_sdp_msg.attributes)
{
}

int AmSdp::parse(const char* _sdp_msg)
{
  char* s = (char*)_sdp_msg;
  clear();
  
  bool ret = parse_sdp_line_ex(this,s);
  
  if(!ret && conn.address.empty()){
    for(vector<SdpMedia>::iterator it = media.begin();
	!ret && (it != media.end()); ++it)
      ret = it->conn.address.empty();
    
    if(ret){
      ERROR("A connection field must be field must be present in every\n");
      ERROR("media description or at the session level.\n");
    }
  }
  
    
  return ret;
}

void AmSdp::print(string& body) const
{
  string out_buf = "v="+int2str(version)+"\r\n"
    "o="+origin.user+" "+int2str(origin.sessId)+" "+
    int2str(origin.sessV)+" IN IP4 ";
  if (!origin.conn.address.empty())
    out_buf += origin.conn.address+"\r\n";
  else if (!conn.address.empty())
    out_buf += conn.address+"\r\n";
  else if (media.size() && !media[0].conn.address.empty())
    out_buf += media[0].conn.address+"\r\n";
  else
    out_buf += "0.0.0.0\r\n";

  out_buf +=
    "s="+sessionName+"\r\n";
  if (!conn.address.empty())
    out_buf += "c=IN IP4 "+conn.address+"\r\n";

  out_buf += "t=0 0\r\n";

  // add attributes (session level)
  for (std::vector<SdpAttribute>::const_iterator a_it=
	 attributes.begin(); a_it != attributes.end(); a_it++) {
    out_buf += a_it->print();
  }

  for(std::vector<SdpMedia>::const_iterator media_it = media.begin();
      media_it != media.end(); media_it++) {
      
      out_buf += "m=" + media_t_2_str(media_it->type) + " " + int2str(media_it->port) + " " + transport_p_2_str(media_it->transport);

      string options;
      for(std::vector<SdpPayload>::const_iterator pl_it = media_it->payloads.begin();
	  pl_it != media_it->payloads.end(); pl_it++) {

	  out_buf += " " + int2str(pl_it->payload_type);
	  if (!media_it->conn.address.empty())
	    options += "c=IN IP4 "+media_it->conn.address+"\r\n";

	  // "a=rtpmap:" line
	  if (!pl_it->encoding_name.empty()) {
	    options += "a=rtpmap:" + int2str(pl_it->payload_type) + " "
	      + pl_it->encoding_name + "/" + int2str(pl_it->clock_rate);

	    if(pl_it->encoding_param > 0){
	      options += "/" + int2str(pl_it->encoding_param);
	    }

	    options += "\r\n";
	  }
	  
	  // "a=fmtp:" line
	  if(pl_it->sdp_format_parameters.size()){
	      options += "a=fmtp:" + int2str(pl_it->payload_type) + " "
		  + pl_it->sdp_format_parameters + "\r\n";
	  }
	  
      }

      out_buf += "\r\n" + options;

      switch(media_it->dir){
      case SdpMedia::DirActive:
	  out_buf += "a=direction:active\r\n";
	  break;
      case SdpMedia::DirPassive:
	  out_buf += "a=direction:passive\r\n";
	  break;
      default: break;
      }

      if(media_it->send){
	if(media_it->recv){
	  out_buf += "a=sendrecv\r\n";
	}
	else {
	  out_buf += "a=sendonly\r\n";
	}
      }
      else {
	if(media_it->recv){
	  out_buf += "a=recvonly\r\n";
	}
	else {
	  out_buf += "a=inactive\r\n";
	}
      }

      // add attributes (media level)
      for (std::vector<SdpAttribute>::const_iterator a_it=
	     media_it->attributes.begin(); a_it != media_it->attributes.end(); a_it++) {
	out_buf += a_it->print();
      }

  }

  body = out_buf;
}

const SdpPayload* AmSdp::telephoneEventPayload() const
{
  return findPayload("telephone-event");
}

const SdpPayload *AmSdp::findPayload(const string& name) const
{
  vector<SdpMedia>::const_iterator m_it;

  for (m_it = media.begin(); m_it != media.end(); ++m_it)
    {
      vector<SdpPayload>::const_iterator it = m_it->payloads.begin();
      for(; it != m_it->payloads.end(); ++it)

	{
	  if (it->encoding_name == name)
	    {
	      return new SdpPayload(*it);
	    }
	}
    }
  return NULL;
}

bool AmSdp::operator == (const AmSdp& other) const
{
  if (attributes.empty()) {
    if (!other.attributes.empty())
      return false;

  } else if (other.attributes.empty()) {
    return false;

  } else {
    std::pair<vector<SdpAttribute>::const_iterator, vector<SdpAttribute>::const_iterator> a_mismatch
      = std::mismatch(attributes.begin(), attributes.end(), other.attributes.begin());

    if (a_mismatch.first != attributes.end() || a_mismatch.second != other.attributes.end())
      return false;
  }
   
  if (media.empty()) {
    if (!other.media.empty())
      return false;

  } else if (other.media.empty()) {
    return false;

  } else {
    std::pair<vector<SdpMedia>::const_iterator, vector<SdpMedia>::const_iterator> m_mismatch
      = std::mismatch(media.begin(), media.end(), other.media.begin());

    if (m_mismatch.first != media.end() || m_mismatch.second != other.media.end())
      return false;
  }

  return version == other.version && origin == other.origin 
    && sessionName == other.sessionName && uri == other.uri && conn == other.conn;
}

void AmSdp::clear()
{
  version = 0;
  origin  = SdpOrigin();
  sessionName.clear();
  uri.clear();
  conn = SdpConnection();
  attributes.clear();
  media.clear();
  l_origin = SdpOrigin();
}

void SdpMedia::calcAnswer(const AmPayloadProvider* payload_prov,
			  SdpMedia& answer) const
{
  if(!recv) answer.send = false;
  if(!send) answer.recv = false;

  switch(dir){
  case SdpMedia::DirBoth:
    answer.dir = SdpMedia::DirBoth;
    break;
  case SdpMedia::DirActive:
    answer.dir = SdpMedia::DirPassive;
    break;
  case SdpMedia::DirPassive:
    answer.dir = SdpMedia::DirActive;
    break;
  }

  // Calculate the intersection with the offered set of payloads
  vector<SdpPayload>::const_iterator it = payloads.begin();
  for(; it!= payloads.end(); ++it) {
    amci_payload_t* a_pl = NULL;
    if(it->payload_type < DYNAMIC_PAYLOAD_TYPE_START) {
      // try static payloads
      a_pl = payload_prov->payload(it->payload_type);
    }

    if( a_pl) {
      answer.payloads.push_back(SdpPayload(a_pl->payload_id,
					   a_pl->name,
					   a_pl->advertised_sample_rate,
					   0));
    }
    else {
      // Try dynamic payloads
      // and give a chance to broken
      // implementation using a static payload number
      // for dynamic ones.

      int int_pt = payload_prov->
	getDynPayload(it->encoding_name,
		      it->clock_rate,
		      it->encoding_param);
      if(int_pt != -1){
	answer.payloads.push_back(*it);
      }
    }
  }
}

//parser
static bool parse_sdp_line_ex(AmSdp* sdp_msg, char*& s)
{

  char* next=0;
  register parse_st state;
  //default state
  state=SDP_DESCR;
  DBG("parsing SDP message...\n");

  while(*s != '\0'){
    switch(state){
    case SDP_DESCR:
      switch(*s){
      case 'v':
	{
	  s = is_eql_next(s);
	  next = get_next_line(s);
	  if (int(next-s)-2 >= 0) {
	    string version(s, int(next-s)-2);
	    str2i(version, sdp_msg->version);
	    //DBG("parse_sdp_line_ex: found version\n");
	  }
	  s = next;
	  state = SDP_DESCR;
	  break;
	  
	}
      case 'o':
	//DBG("parse_sdp_line_ex: found origin\n");
	s = is_eql_next(s);
	parse_sdp_origin(sdp_msg, s);
	s = get_next_line(s);
	state = SDP_DESCR;
	break;
      case 's':
	{
	  //DBG("parse_sdp_line_ex: found session\n");
	  s = is_eql_next(s);
	  next = get_next_line(s);
	  if (int(next-s)-2 >= 0) {
	    string sessionName(s, int(next-s)-2);
	    sdp_msg->sessionName = sessionName;
	  }
	  s = next;
	  break;
	}

      case 'u': {
	//DBG("parse_sdp_line_ex: found uri\n");
	  s = is_eql_next(s);
	  next = get_next_line(s);
	  if (int(next-s)-2 >= 0) {
	    sdp_msg->uri = string(s, int(next-s)-2);
	  }
	  s = next;
      } break;

      case 'i':
      case 'e':
      case 'p':
      case 'b':
      case 't':
      case 'k':
	//DBG("parse_sdp_line_ex: found unknown line '%c'\n", *s);
	s = is_eql_next(s);
	next = get_next_line(s);
	s = next;
	state = SDP_DESCR;
	break;
      case 'a':
	//DBG("parse_sdp_line_ex: found attributes\n");
	s = is_eql_next(s);
	parse_session_attr(sdp_msg, s, &next);
	// next = get_next_line(s);
	// parse_sdp_attr(sdp_msg, s);
	s = next;
	state = SDP_DESCR;
	break;
      case 'c':
	//DBG("parse_sdp_line_ex: found connection\n");
	s = is_eql_next(s);
	parse_sdp_connection(sdp_msg, s, 'd');
	s = get_next_line(s);
	state = SDP_DESCR;	
	break;
      case 'm':
	//DBG("parse_sdp_line_ex: found media\n");
	state = SDP_MEDIA;	
	break;

      default:
	{
	  next = get_next_line(s);
	  if (int(next-s)-2 >= 0) {
	    string line(s, int(next-s)-2);
	    DBG("parse_sdp_line: skipping unknown Session description %s=\n",
		(char*)line.c_str());
	  }
	  s = next;
	  break;
	}
      }
      break;

    case SDP_MEDIA:
      switch(*s){
      case 'm':
	s = is_eql_next(s);
	parse_sdp_media(sdp_msg, s);
	s = get_next_line(s);
	state = SDP_MEDIA;
	break;
      case 'i':
	s = is_eql_next(s);
	s = get_next_line(s);
	state = SDP_MEDIA;
	break;
      case 'c':
	s = is_eql_next(s);
	//DBG("parse_sdp_line: found media connection\n");
	parse_sdp_connection(sdp_msg, s, 'm');
	s = get_next_line(s);
	state = SDP_MEDIA;
	break;
      case 'b':
	s = is_eql_next(s);
	s = get_next_line(s);
	state = SDP_MEDIA;
	break;
      case 'k':
	s = is_eql_next(s);
	s = get_next_line(s);
	state = SDP_MEDIA;
	break;
      case 'a':
	s = is_eql_next(s);
	parse_sdp_attr(sdp_msg, s);
	s = get_next_line(s);
	state = SDP_MEDIA;
	break;
	
      default :
	{
	  next = get_next_line(s);
	  if (int(next-s)-2 >= 0) {
	    string line(s, int(next-s)-2);
	    DBG("parse_sdp_line: skipping unknown Media description '%s'\n",
		(char*)line.c_str());
	  }
	  s = next;
	  break;
	}
      }
      break;
    }
  }

  return false;
}


static void parse_sdp_connection(AmSdp* sdp_msg, char* s, char t)
{
  
  char* connection_line=s;
  char* next=0;
  char* line_end=0;
  int parsing=1;

  SdpConnection c;

  line_end = get_next_line(s);
  register sdp_connection_st state;
  state = NET_TYPE;

  //DBG("parse_sdp_line_ex: parse_sdp_connection: parsing sdp connection\n");

  while(parsing){
    switch(state){
    case NET_TYPE:
      //Ignore NET_TYPE since it is always IN 
      connection_line +=3;
      state = ADDR_TYPE;
      break;
    case ADDR_TYPE:
      {
	string addr_type(connection_line,3);
	connection_line +=4;
	if(addr_type == "IP4"){
	  c.addrType = 1;
	  state = IP4;
	}else if(addr_type == "IP6"){
	  c.addrType = 2;
	  state = IP6;
	}else{
	  ERROR("parse_sdp_connection: Unknown addr_type in c=\n");
	  c.addrType = 0;
	  parsing = 0;
	}
	break;
      }
    case IP4:
      {
	  if(contains(connection_line, line_end, '/')){
	      next = parse_until(s, '/');
	      c.address = string(connection_line,int(next-connection_line)-2);
	  }else{
	      c.address = string(connection_line, int(line_end-connection_line)-2);
	  }
	  parsing = 0;
	  break;
      }
      
    case IP6:
      { 
	  if(contains(connection_line, line_end, '/')){
	      next = parse_until(s, '/');
	      c.address = string(connection_line, int(next-connection_line)-2);
	  }else{
	      c.address = string(connection_line, int(line_end-connection_line)-2);
	  }
	  parsing = 0;
	  break;
      }
    }
  }
  if(t == 'd') {
    sdp_msg->conn = c;
    DBG("SDP: got session level connection: %s\n", c.debugPrint().c_str());
  } else if(t == 'm'){
    SdpMedia& media = sdp_msg->media.back();
    media.conn = c;
    DBG("SDP: got media level connection: %s\n", c.debugPrint().c_str());
  }

  //DBG("parse_sdp_line_ex: parse_sdp_connection: done parsing sdp connection\n");
  return;
}


static void parse_sdp_media(AmSdp* sdp_msg, char* s)
{
  SdpMedia m;
  
  register sdp_media_st state;
  state = MEDIA;
  int parsing = 1;
  char* media_line=s;
  char* next=0;
  char* line_end=0;
  line_end = get_next_line(media_line);
  SdpPayload payload;
  unsigned int payload_type;

  //DBG("parse_sdp_line_ex: parse_sdp_media: parsing media description...\n");
  m.dir = SdpMedia::DirBoth;

  while(parsing){
    switch(state){
    case MEDIA: 
      {
      next = parse_until(media_line, ' ');
      string media;
      if (next > media_line)
	media = string(media_line, int(next-media_line)-1);
      m.type = media_type(media);
      if(m.type < 0) {
	ERROR("parse_sdp_media: Unknown media type\n");
      }
      media_line = next;
      state = PORT;
      break;
      }
    case PORT:
      {
      next = parse_until(media_line, ' ');
      //check for multiple ports
      if(contains(media_line, next, '/')){
	//port number
	next = parse_until(media_line, '/');
	string port;
	if (next > media_line)
	  port = string(media_line, int(next-media_line)-1);
	str2i(port, m.port);	
	//number of ports
	media_line = next;
	next = parse_until(media_line, ' ');
	string nports;
	if (next > media_line)
	  nports = string(media_line, int(next-media_line)-1);
	str2i(nports, m.nports);
      }else{
	//port number 
	next = parse_until(media_line, ' ');
	string port;
	if (next > media_line)
	  port = string(media_line, int(next-media_line)-1);
	str2i(port, m.port);
	media_line = next;
      }
      state = PROTO;
      break;
      }
    case PROTO:
      {
	next = parse_until(media_line, ' ');
	string proto;
	if (next > media_line)
	  proto = string(media_line, int(next-media_line)-1);
	// if(transport_type(proto) < 0){
	//   ERROR("parse_sdp_media: Unknown transport protocol\n");
	//   state = FMT;
	//   break;
	// }
	m.transport = transport_type(proto);
	if(m.transport < 0){
	  DBG("Unknown transport protocol: %s\n",proto.c_str());
	}
	media_line = next;
	state = FMT;
	break;
      }
    case FMT:
      {
	if (contains(media_line, line_end, ' ')) {
	  next = parse_until(media_line, ' ');
	  string value;
	  if (next > media_line)
	    value = string(media_line, int(next-media_line)-1);

	  if (!value.empty()) {
	    payload.type = m.type;
	    str2i(value, payload_type);
	    payload.payload_type = payload_type;
	    m.payloads.push_back(payload);
	  }

	  media_line = next;
	  state = FMT;
	} else {
	  string last_value;
	  if (line_end>media_line) {
	    if (*line_end == '\0') {
	      // last line in message
	      last_value = string(media_line, int(line_end-media_line));
	    } else {
	      last_value = string(media_line, int(line_end-media_line)-1);
	    }
	  }
	  if (!last_value.empty()) {
	    payload.type = m.type;
	    str2i(last_value, payload_type);
	    payload.payload_type = payload_type;
	    m.payloads.push_back(payload);
	  }
	  parsing = 0;
	}
	break;
      }
    }
  }
  sdp_msg->media.push_back(m);

  DBG("SDP: got media: %s\n", m.debugPrint().c_str());
  //DBG("parse_sdp_line_ex: parse_sdp_media: done parsing media description \n");
  return;
}

// session level attribute
static void parse_session_attr(AmSdp* sdp_msg, char* s, char** next) {
  *next = get_next_line(s);
  if (*next == s) {
    WARN("premature end of SDP in session attr\n");
    while (**next != '\0') (*next)++;
    return;
  }
  char* attr_end = *next-1;
  while (attr_end >= s &&
	 ((*attr_end == 10) || (*attr_end == 13)))
    attr_end--;

  if (*attr_end == ':') {
    WARN("incorrect SDP: value attrib without value: '%s'\n",
	 string(s, attr_end-s+1).c_str());
    return;
  }

  char* col = parse_until(s, attr_end, ':');

  if (col == attr_end) {
    // property attribute
    sdp_msg->attributes.push_back(SdpAttribute(string(s, attr_end-s+1)));
    // DBG("got session attribute '%.*s\n", (int)(attr_end-s+1), s);
  } else {
    // value attribute
    sdp_msg->attributes.push_back(SdpAttribute(string(s, col-s-1),
					       string(col, attr_end-col+1)));
    // DBG("got session attribute '%.*s:%.*s'\n", (int)(col-s-1), s, (int)(attr_end-col+1), col);
  }

}

// media level attribute
static void parse_sdp_attr(AmSdp* sdp_msg, char* s)
{
  if(sdp_msg->media.empty()){
    ERROR("While parsing media options: no actual media !\n");
    return;
  }
  
  SdpMedia& media = sdp_msg->media.back();

  SdpPayload payload;

  register sdp_attr_rtpmap_st rtpmap_st;
  register sdp_attr_fmtp_st fmtp_st;
  rtpmap_st = TYPE;
  fmtp_st = FORMAT;
  char* attr_line=s;
  char* next=0;
  char* line_end=0;
  int parsing = 1;
  line_end = get_next_line(attr_line);
  
  unsigned int payload_type, clock_rate, encoding_param = 0;
  string encoding_name, params;

  string attr;
  if (!contains(attr_line, line_end, ':')) {
    next = parse_until(attr_line, '\r');
    if (next >= line_end) {
      DBG("found attribute line '%s', which is not followed by cr\n", attr_line);
      next = line_end;
    }
    attr = string(attr_line, int(next-attr_line)-1);
    attr_check(attr);
    parsing = 0;
  } else {
    next = parse_until(attr_line, ':');
    attr = string(attr_line, int(next-attr_line)-1);
    attr_line = next;
  }

  if(attr == "rtpmap"){
    while(parsing){
      switch(rtpmap_st){
      case TYPE:
	{
	  next = parse_until(attr_line, ' ');
	  string type(attr_line, int(next-attr_line)-1);
	  str2i(type,payload_type);
	  attr_line = next;
	  rtpmap_st = ENC_NAME;
	  break;
	}
      case ENC_NAME:
	{
	  if(contains(s, line_end, '/')){
	    next = parse_until(attr_line, '/');
	    string enc_name(attr_line, int(next-attr_line)-1);
	    encoding_name = enc_name;
	    attr_line = next;
	    rtpmap_st = CLK_RATE;
	    break;
	  } else {
	    rtpmap_st = ENC_PARAM;
	    break;
	  }
	}
      case CLK_RATE:
	{
	  // check for posible encoding parameters after clock rate
	  if(contains(attr_line, line_end, '/')){
	    next = parse_until(attr_line, '/');
	    string clk_rate(attr_line, int(next-attr_line)-1);
	    str2i(clk_rate, clock_rate);
	    attr_line = next;
	    rtpmap_st = ENC_PARAM;
	    //last line check
	  }else if (*line_end == '\0') {
	    string clk_rate(attr_line, int(line_end-attr_line));
	    str2i(clk_rate, clock_rate);
	    parsing = 0;
	    //more lines to come
	  }else{
	    string clk_rate(attr_line, int(line_end-attr_line)-1);
	    str2i(clk_rate, clock_rate);
	    parsing=0;
	  }
	    
	  break;
	}
      case ENC_PARAM:
	{
	  next = parse_until(attr_line, ' ');
	  if(next < line_end){
	    string value(attr_line, int(next-attr_line)-1);
	    str2i(value, encoding_param);
	    attr_line = next;
	    rtpmap_st = ENC_PARAM;
	  }else{
	    string last_value(attr_line, int(line_end-attr_line)-1);
	    str2i(last_value, encoding_param);
	    parsing = 0;
	  }
	  break;
	}
	break;
      }
    }
      
    //DBG("found media attr 'rtpmap' type '%d'\n", payload_type);
      
    vector<SdpPayload>::iterator pl_it;
      
    for( pl_it=media.payloads.begin();
	 (pl_it != media.payloads.end())
	   && (pl_it->payload_type != int(payload_type));
	 ++pl_it);

    if(pl_it != media.payloads.end()){
      *pl_it = SdpPayload( int(payload_type),
			   encoding_name,
			   int(clock_rate),
			   int(encoding_param));
    }

  } else if(attr == "fmtp"){
    while(parsing){
      switch(fmtp_st){
      case FORMAT:
	{
	  next = parse_until(attr_line, ' ');
	  string fmtp_format(attr_line, int(next-attr_line)-1);
	  str2i(fmtp_format, payload_type);
	  attr_line = next;
	  fmtp_st = FORMAT_PARAM;
	  break;
	}
      case FORMAT_PARAM:
	{ 
	  line_end--;
	  while (is_wsp(*line_end))
	    line_end--;

	  params = string(attr_line, line_end-attr_line+1);
	  parsing = 0;
	}
	break;
      }
    }

    //DBG("found media attr 'fmtp' for payload '%d': '%s'\n", 
    //  payload_type, params.c_str());

    vector<SdpPayload>::iterator pl_it;
      
    for(pl_it=media.payloads.begin();
	(pl_it != media.payloads.end())
	  && (pl_it->payload_type != int(payload_type));
	pl_it++);

    if(pl_it != media.payloads.end())
      pl_it->sdp_format_parameters = params;

  } else if (attr == "direction") {
    if (parsing) {
      next = parse_until(attr_line, '\r');
      if(next < line_end){
	string value(attr_line, int(next-attr_line)-1);
	if (value == "active") {
	  media.dir=SdpMedia::DirActive;
	  // DBG("found media attr 'direction' value '%s'\n", (char*)value.c_str());
	} else if (value == "passive") {
	  media.dir=SdpMedia::DirPassive;
	  //DBG("found media attr 'direction' value '%s'\n", (char*)value.c_str());
	} else if (attr == "both") {
	  media.dir=SdpMedia::DirBoth;
	  //DBG("found media attr 'direction' value '%s'\n", (char*)value.c_str());
	} 
      } else {
	DBG("found media attribute 'direction', but value is not followed by cr\n");
      }
    } else {
      DBG("ignoring direction attribute without value\n");
    }
  } else if (attr == "sendrecv") {
    media.send = true;
    media.recv = true;
  } else if (attr == "sendonly") {
    media.send = true;
    media.recv = false;
  } else if (attr == "recvonly") {
    media.send = false;
    media.recv = true;
  } else if (attr == "inactive") {
    media.send = false;
    media.recv = false;
  } else {
    attr_check(attr);
    string value;
    if (parsing) {
      next = parse_until(attr_line, '\r');
      if(next >= line_end){
	DBG("found media attribute '%s', but value is not followed by cr\n",
	    (char *)attr.c_str());
      }
      value = string (attr_line, int(next-attr_line)-1);
    }

    // if (value.empty()) {
    //   DBG("got media attribute '%s'\n", attr.c_str());
    // } else {
    //   DBG("got media attribute '%s':'%s'\n", attr.c_str(), value.c_str());
    // }
    media.attributes.push_back(SdpAttribute(attr, value));
  }
}

static void parse_sdp_origin(AmSdp* sdp_msg, char* s)
{
  char* origin_line = s;
  char* next=0;
  char* line_end=0;
  line_end = get_next_line(s);
  
  register sdp_origin_st origin_st;
  origin_st = USER;
  int parsing = 1;
  
  SdpOrigin origin;

  //DBG("parse_sdp_line_ex: parse_sdp_origin: parsing sdp origin\n");

  while(parsing){
    switch(origin_st)
      {
      case USER:
	{
	  next = parse_until(origin_line, ' ');
	  if(next > line_end){
	    DBG("parse_sdp_origin: ST_USER: Incorrect number of value in o=\n");
	    origin_st = UNICAST_ADDR;
	    break;
	  }
	  string user(origin_line, int(next-origin_line)-1);
	  origin.user = user;
	  origin_line = next;
	  origin_st = ID;
	  break;
	}
      case ID:
	{
	  next = parse_until(origin_line, ' ');
	  if(next > line_end){
	    DBG("parse_sdp_origin: ST_ID: Incorrect number of value in o=\n");
	    origin_st = UNICAST_ADDR;
	    break;
	  }
	  string id(origin_line, int(next-origin_line)-1);
	  str2i(id, origin.sessId);
	  origin_line = next;
	  origin_st = VERSION_ST;
	  break;
	}
      case VERSION_ST:
	{
	  next = parse_until(origin_line, ' ');
	  if(next > line_end){
	    DBG("parse_sdp_origin: ST_VERSION: Incorrect number of value in o=\n");
	    origin_st = UNICAST_ADDR;
	    break;
	  }
	  string version(origin_line, int(next-origin_line)-1);
	  str2i(version, origin.sessV);
	  origin_line = next;
	  origin_st = NETTYPE;
	  break;
	}
      case NETTYPE:
	{
	  next = parse_until(origin_line, ' ');
	  if(next > line_end){
	    DBG("parse_sdp_origin: ST_NETTYPE: Incorrect number of value in o=\n");
	    origin_st = UNICAST_ADDR;
	    break;
	  }
	  string net_type(origin_line, int(next-origin_line)-1);
	  origin_line = next;
	  origin_st = ADDR;
	  break;
	}
      case ADDR:
	{
       	  next = parse_until(origin_line, ' ');
	  if(next > line_end){
	    DBG("parse_sdp_origin: ST_ADDR: Incorrect number of value in o=\n");
	    origin_st = UNICAST_ADDR;
	    break;
	  }
	  string addr_type(origin_line, int(next-origin_line)-1);
	  origin_line = next;
	  origin_st = UNICAST_ADDR;
	  break;
	}
      case UNICAST_ADDR:
	{
	  next = parse_until(origin_line, ' ');
	  //check if line contains more values than allowed
	  if(next > line_end){
	    origin.conn.address = string(origin_line, int(line_end-origin_line)-2);
	  }else{
	    DBG("parse_sdp_origin: 'o=' contains more values than allowed; these values will be ignored\n");  
	    origin.conn.address = string(origin_line, int(next-origin_line)-1);
	  }
	  parsing = 0;
	  break;
	}
      }
  }
  
  sdp_msg->origin = origin;

  //DBG("parse_sdp_line_ex: parse_sdp_origin: done parsing sdp origin\n");
  return;
}


/*
 *HELPER FUNCTIONS
 */

static bool contains(char* s, char* next_line, char c)
{
  char* line=s;
  while((line != next_line-1) && (*line)){
    if(*line == c)
      return true;
    line++;
  }
  return false;
}

static bool is_wsp(char s) {
  return s==' ' || s == '\r' || s == '\n' || s == '\t';
}

static char* parse_until(char* s, char end)
{
  char* line=s;
  while(*line && *line != end ){
    line++;
  }
  line++;
  return line;
}

static char* parse_until(char* s, char* end, char c)
{
  char* line=s;
  while(line<end && *line && *line != c ){
    line++;
  }
  if (line<end)
    line++;
  return line;
}

static char* is_eql_next(char* s)
{
  char* current_line=s;
  if(*(++current_line) != '='){
    DBG("parse_sdp_line: expected '=' but found <%c> \n", *current_line);
  }
  current_line +=1;
  return current_line;
}

inline char* get_next_line(char* s)
{
  char* next_line=s;
  //search for next line
 while( *next_line != '\0') {
    if(*next_line == 13){
      next_line +=2;
      break;
    }
    else if(*next_line == 10){	
      next_line +=1;
      break;
    }  
    next_line++;
 }

  return next_line; 
}


/*
 *Check if known media type is used
 */
static int media_type(std::string media)
{
  if(media == "audio")
    return 1;
  else if(media == "video")
    return 2;
  else if(media == "application")
    return 3;
  else if(media == "text")
    return 4;
  else if(media == "message")
    return 5;
  else 
    return -1;
}

static int transport_type(std::string transport)
{
  if(transport == "RTP/AVP")
    return 1;
  else if(transport == "UDP")
    return 2;
  else if(transport == "RTP/SAVP")
    return 3;
  else 
    return -1;
}

/*
*Check if known attribute name is used
*/
static bool attr_check(std::string attr)
{
  if(attr == "cat")
    return true;
  else if(attr == "keywds")
    return true;
  else if(attr == "tool")
    return true;
  else if(attr == "ptime")
    return true;
  else if(attr == "maxptime")
    return true;
  else if(attr == "recvonly")
    return true;
  else if(attr == "sendrecv")
    return true;
  else if(attr == "sendonly")
    return true;
  else if(attr == "inactive")
    return true;
  else if(attr == "orient")
    return true;
  else if(attr == "type")
    return true;
  else if(attr == "charset")
    return true;
  else if(attr == "sdplang")
    return true;
  else if(attr == "lang")
    return true;
  else if(attr == "framerate")
    return true;
  else if(attr == "quality")
    return true;
  else if(attr == "both")
    return true;
  else if(attr == "active")
    return true;
  else if(attr == "passive")
    return true;
  else {
    DBG("unknown attribute: %s\n", (char*)attr.c_str());
    return false;
  }
}

