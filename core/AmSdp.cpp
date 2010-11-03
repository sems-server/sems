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

bool SdpPayload::operator == (int r)
{
  DBG("pl == r: payload_type = %i; r = %i\n", payload_type, r);
  return payload_type == r;
}

string SdpAttribute::print() const {
  if (value.empty())
    return "a="+attribute+CRLF;
  else
    return "a="+attribute+":"+value+CRLF;
}

//
// class AmSdp: Methods
//
AmSdp::AmSdp()
  : remote_active(false),
    telephone_event_pt(NULL),
    accepted_media(0)
{
  l_origin.user = "sems";
  l_origin.sessId = get_random();
  l_origin.sessV = get_random();
}

AmSdp::AmSdp(const AmSdp& p_sdp_msg)
  : version(p_sdp_msg.version),
    origin(p_sdp_msg.origin),
    l_origin(p_sdp_msg.l_origin),
    sessionName(p_sdp_msg.sessionName),
    conn(p_sdp_msg.conn),
    media(p_sdp_msg.media),
    telephone_event_pt(NULL),
    remote_active(false),
    accepted_media(0)
{
  memcpy(r_buf,p_sdp_msg.r_buf,BUFFER_SIZE);
}


void AmSdp::setBody(const char* _sdp_msg)
{
  if (!memchr(_sdp_msg, '\0', BUFFER_SIZE)) {
    throw AmSession::Exception(513, "Message too big");
  }
  strcpy(r_buf, _sdp_msg);
}

int AmSdp::parse()
{
  char* s = r_buf;
  media.clear();
  
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
  
  telephone_event_pt = findPayload("telephone-event");
    
  return ret;
}

void AmSdp::print(string& body) const
{
  string out_buf =
      "v="+int2str(version)+"\r\n"
      "o="+origin.user+" "+int2str(origin.sessId)+" "+int2str(origin.sessV)+" IN IP4 "+conn.address+"\r\n"
      "s="+sessionName+"\r\n"
      "c=IN IP4 "+conn.address+"\r\n"
      "t=0 0\r\n";

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

	  // "a=rtpmap:" line
	  options += "a=rtpmap:" + int2str(pl_it->payload_type) + " " 
	      + pl_it->encoding_name + "/" + int2str(pl_it->clock_rate);

	  if(pl_it->encoding_param > 0){
	      options += "/" + int2str(pl_it->encoding_param);
	  }

	  options += "\r\n";
	  
	  // "a=fmtp:" line
	  if(pl_it->sdp_format_parameters.size()){
	      options += "a=fmtp:" + int2str(pl_it->payload_type) + " "
		  + pl_it->sdp_format_parameters + "\r\n";
	  }
	  
      }

      out_buf += "\r\n" + options;

      if(remote_active /* dir == SdpMedia::DirActive */)
	  out_buf += "a=direction:passive\r\n";

      // add attributes (media level)
      for (std::vector<SdpAttribute>::const_iterator a_it=
	     media_it->attributes.begin(); a_it != media_it->attributes.end(); a_it++) {
	out_buf += a_it->print();
      }

  }

  body = out_buf;
  //mime_type = "application/sdp";
}

int AmSdp::genResponse(const string& localip, int localport, string& out_buf, bool single_codec)
{
  string l_ip = "IP4 " + localip;
  l_origin.sessV++; 

#ifdef SUPPORT_IPV6
  if(l_ip.find('.') == string::npos)
    l_ip = "IP6" + localip;
#endif
  
  out_buf =
  "v=0\r\n"
  "o="+l_origin.user+" "+int2str(l_origin.sessId)+" "+int2str(l_origin.sessV)+" IN " + l_ip + "\r\n"
  "s=session\r\n"
  "c=IN " + l_ip + "\r\n"
  "t=0 0\r\n"
  "m=audio " + int2str(localport) + " RTP/AVP";
 
 string payloads;
 string options;
 
 for(vector<SdpPayload*>::iterator it = sup_pl.begin();
     it != sup_pl.end(); ++it){
   payloads += " " + int2str((*it)->payload_type);
   if((*it)->encoding_param <= 0){
     options += "a=rtpmap:" + int2str((*it)->payload_type) + " "
       + (*it)->encoding_name + "/" + int2str((*it)->clock_rate) + "\r\n";
   }else{
     options += "a=rtpmap:" + int2str((*it)->payload_type) + " "
       + (*it)->encoding_name + "/" + int2str((*it)->clock_rate) + "/" + int2str((*it)->encoding_param) + "\r\n";

   }

   if((*it)->sdp_format_parameters.size()){
     options += "a=fmtp:" + int2str((*it)->payload_type) + " "
       + (*it)->sdp_format_parameters + "\r\n";
   }
   if (single_codec) break;
 }
 
 if (hasTelephoneEvent())
   payloads += " " + int2str(telephone_event_pt->payload_type);
 
 out_buf += payloads + "\r\n"
  +options;
 
 if(hasTelephoneEvent())
   {
     out_buf += "a=rtpmap:" + int2str(telephone_event_pt->payload_type) + " " + 
       telephone_event_pt->encoding_name + "/" +
       int2str(telephone_event_pt->clock_rate) + "\r\n"
       "a=fmtp:" + int2str(telephone_event_pt->payload_type) + " 0-15\r\n";
   }
 
 if(remote_active /* dir == SdpMedia::DirActive */)
   out_buf += "a=direction:passive\r\n";

  // add rejected media line for all except the accepted one
  for( vector<SdpMedia>::iterator m_it = media.begin(); m_it != media.end(); ++m_it ){
    if ((unsigned int)(m_it - media.begin())  != accepted_media) {
      string rej_line = "m=" + media_t_2_str(m_it->type) + " 0 " +
	transport_p_2_str(m_it->transport);
      // add one bogus payload (required by sdp) - ignored (3264 s 6)
      if (m_it->payloads.size()) {
	rej_line += " "+int2str(m_it->payloads[0].payload_type)+"\n";
      } else {
	rej_line += " 0\n"; // violating 3264, but in this case the offer already did
      }
      out_buf += rej_line;
    }
  }
 
 return 0;
}

int AmSdp::genRequest(const string& localip, int localport, string& out_buf)
{
  AmPlugIn* plugin = AmPlugIn::instance();
  const map<int, amci_payload_t*>& payloads = plugin->getPayloads();
  const map<int,int>& payload_order = plugin->getPayloadOrder();

  if(payloads.empty()){
    ERROR("no payload plugin loaded.\n");
    return -1;
  }

  l_origin.sessV++; 

  string l_ip = "IP4 " + localip;

#ifdef SUPPORT_IPV4
  if(l_ip.find('.') == string::npos)
    l_ip = "IP6" + localip;
#endif

  out_buf = 
    "v=0\r\n"
    "o="+l_origin.user+" "+int2str(l_origin.sessId)+" "+int2str(l_origin.sessV)+" IN " + l_ip + "\r\n"
    "s=session\r\n"
    "c=IN " + l_ip + "\r\n"
    "t=0 0\r\n"
    "m=audio " + int2str(localport) + " RTP/AVP ";

  std::map<int,int>::const_iterator it = payload_order.begin();
  out_buf += int2str((it++)->second);

  for(; it != payload_order.end(); ++it)
    out_buf += string(" ") + int2str(it->second);

  out_buf += "\r\n";
  
  for (it = payload_order.begin(); it != payload_order.end(); ++it) {
    map<int,amci_payload_t*>::const_iterator it2 = payloads.find(it->second);
    if(it2 != payloads.end()){
      out_buf += "a=rtpmap:" + int2str(it2->first)
	+ " " + string(it2->second->name)
	+ "/" + int2str(it2->second->sample_rate)
	+ "\r\n";
      if (it2->second->name == string("telephone-event")) {
	out_buf +="a=fmtp:"+int2str(it2->first)+" 0-16\r\n";
      }
    } else {
      ERROR("Payload %d was not found in payloads map!\n", it->second);
      return -1;
    }
  }
  return 0;

}

const vector<SdpPayload*>& AmSdp::getCompatiblePayloads(AmPayloadProviderInterface* payload_provider, 
							int media_type, string& addr, int& port)
{
  vector<SdpMedia>::iterator m_it;
  SdpPayload *payload;
  sup_pl.clear();

  for(m_it = media.begin(); m_it != media.end(); ++m_it){
    //DBG("type found: %d\n", m_it->type);
    //DBG("media port: %d\n", m_it->port);
    //DBG("media transport: %d\n", m_it->transport);
    //    for(int i=0; i < 8 ; i++){
    //  DBG("media payloads: %d\n", m_it->payloads[i].payload_type);
    //  DBG("media payloads: %s\n", m_it->payloads[i].encoding_name.c_str());
    //  DBG("media clock rates: %d\n", m_it->payloads[i].clock_rate);
    //}
    //    DBG("type found: %d\n", m_it->payloads[0].t);

    // only accept our media type, and reject if port=0 (section 8.2)
    if( (media_type != m_it->type) || (!m_it->port))
      continue;

    vector<SdpPayload>::iterator it = m_it->payloads.begin();
    for(; it!= m_it->payloads.end(); ++it) {
           amci_payload_t* a_pl = NULL;
      if(it->payload_type < DYNAMIC_PAYLOAD_TYPE_START) {
	// try static payloads
	a_pl = payload_provider->payload(it->payload_type);
      }

      if( a_pl) {
	payload  = &(*it);
	payload->int_pt = a_pl->payload_id;
	payload->encoding_name = a_pl->name;
	payload->clock_rate = a_pl->sample_rate;
	sup_pl.push_back(payload);
      }
      else {
	// Try dynamic payloads
	// and give a chance to broken
	// implementation using a static payload number
	// for dynamic ones.
	if(it->encoding_name == "telephone-event")
	  continue;

	int int_pt = payload_provider->
	  getDynPayload(it->encoding_name,
			it->clock_rate,
			it->encoding_param);
	if(int_pt != -1){
	  payload = &(*it);
	  payload->int_pt = int_pt;
	  sup_pl.push_back(payload);
	}
      }
    }
    if( sup_pl.size() > 0)
    {
      if(m_it->conn.address.empty())
	{
	  DBG("using global address: %s\n", m_it->conn.address.c_str());
	  addr = conn.address;
	}
      else {
	DBG("using media specific address: %s\n", m_it->conn.address.c_str());
	addr = m_it->conn.address;
      }
      
      if(m_it->dir == SdpMedia::DirActive)
	remote_active = true;
      
      port = (int)m_it->port;

      // save index of accepted media
      accepted_media = m_it - media.begin();
    }
    break;
  }
  return sup_pl;
}
	
bool AmSdp::hasTelephoneEvent()
{
  return telephone_event_pt != NULL;
}

const SdpPayload *AmSdp::findPayload(const string& name)
{
  vector<SdpMedia>::iterator m_it;

  for (m_it = media.begin(); m_it != media.end(); ++m_it)
    {
      vector<SdpPayload>::iterator it = m_it->payloads.begin();
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

//parser
static bool parse_sdp_line_ex(AmSdp* sdp_msg, char*& s)
{

  char* next=0;
  register parse_st state;
  //default state
  state=SDP_DESCR;
  DBG("parse_sdp_line_ex: parsing sdp message ..\n");

  while(*s != '\0'){
    switch(state){
    case SDP_DESCR:
      switch(*s){
      case 'v':
	{
	  s = is_eql_next(s);
	  next = get_next_line(s);
	  string version(s, int(next-s)-2);
	  str2i(version, sdp_msg->version);
	  DBG("parse_sdp_line_ex: found version\n");
	  s = next;
	  state = SDP_DESCR;
	  break;
	  
	}
      case 'o':
	DBG("parse_sdp_line_ex: found origin\n");
	s = is_eql_next(s);
	parse_sdp_origin(sdp_msg, s);
	s = get_next_line(s);
	state = SDP_DESCR;
	break;
      case 's':
	{
	  DBG("parse_sdp_line_ex: found session\n");
	  s = is_eql_next(s);
	  next = get_next_line(s);
	  string sessionName(s, int(next-s)-2);
	  sdp_msg->sessionName = sessionName;
	  s = next;
	  break;
	}

      case 'u': {
	  DBG("parse_sdp_line_ex: found uri\n");
	  s = is_eql_next(s);
	  next = get_next_line(s);
	  sdp_msg->uri = string(s, int(next-s)-2);
	  s = next;
      } break;

      case 'i':
      case 'e':
      case 'p':
      case 'b':
      case 't':
      case 'k':
	DBG("parse_sdp_line_ex: found unknown line '%c'\n", *s);
	s = is_eql_next(s);
	next = get_next_line(s);
	s = next;
	state = SDP_DESCR;
	break;
      case 'a':
	  DBG("parse_sdp_line_ex: found attributes\n");
	s = is_eql_next(s);
	parse_session_attr(sdp_msg, s, &next);
	  // next = get_next_line(s);
	//	parse_sdp_attr(sdp_msg, s);
	s = next;
	state = SDP_DESCR;
	break;
      case 'c':
	DBG("parse_sdp_line_ex: found connection\n");
	s = is_eql_next(s);
	parse_sdp_connection(sdp_msg, s, 'd');
	s = get_next_line(s);
	state = SDP_DESCR;	
	break;
      case 'm':
	DBG("parse_sdp_line_ex: found media\n");
	state = SDP_MEDIA;	
	break;

      default:
	{
	  next = get_next_line(s);
	  string line(s, int(next-s)-2);
	  DBG("parse_sdp_line: skipping unknown Session description %s=\n", (char*)line.c_str());
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
	DBG("parse_sdp_line: found media connection\n");
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
	  string line(s, int(next-s)-2);
	  DBG("parse_sdp_line: skipping unknown Media description '%s'\n", 
	      (char*)line.c_str());
	  s = next;
	  break;
	}
      }
      break;
    }
  }
  DBG("parse_sdp_line_ex: parsing sdp message done :) \n");
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

  DBG("parse_sdp_line_ex: parse_sdp_connection: parsing sdp connection\n");

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
  if(t == 'd')
    sdp_msg->conn = c;
  if(t == 'm'){
    SdpMedia& media = sdp_msg->media.back();
    media.conn = c;
  }

  DBG("parse_sdp_line_ex: parse_sdp_connection: done parsing sdp connection\n");
  return;
}


static void parse_sdp_media(AmSdp* sdp_msg, char* s)
{
  SdpMedia m;
  
  //clear(m);

  register sdp_media_st state;
  state = MEDIA;
  int parsing = 1;
  char* media_line=s;
  char* next=0;
  char* line_end=0;
  line_end = get_next_line(media_line);
  SdpPayload payload;
  unsigned int payload_type;
  DBG("parse_sdp_line_ex: parse_sdp_media: parsing media description...\n");
  m.dir = SdpMedia::DirBoth;

  while(parsing){
    switch(state){
    case MEDIA: 
      {
      next = parse_until(media_line, ' ');
      string media(media_line, int(next-media_line)-1);
      if(media_type(media) < 0 )
	ERROR("parse_sdp_media: Unknown media type\n");
      m.type = media_type(media);
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
	string port(media_line, int(next-media_line)-1);
	str2i(port, m.port);	
	//number of ports
	media_line = next;
	next = parse_until(media_line, ' ');
       	string nports(media_line, int(next-media_line)-1);
	str2i(nports, m.nports);
      }else{
	//port number 
	next = parse_until(media_line, ' ');
	const string port(media_line, int(next-media_line)-1);
	str2i(port, m.port);
	media_line = next;
      }
      state = PROTO;
      break;
      }
    case PROTO:
      {
	next = parse_until(media_line, ' ');
	string proto(media_line, int(next-media_line)-1);
	if(transport_type(proto) < 0){
	  ERROR("parse_sdp_media: Unknown transport protocol\n");
	  state = FMT;
	  break;
	}
	m.transport = transport_type(proto);
	media_line = next;
	state = FMT;
	break;
      }
    case FMT:
      {
	if(contains(media_line, line_end, ' ')){
	  next = parse_until(media_line, ' ');
	//if(next < line_end){
	  string value(media_line, int(next-media_line)-1);
	  media_line = next;
	  payload.type = m.type;
	  str2i(value, payload_type);
	  payload.payload_type = payload_type;
	  m.payloads.push_back(payload);
	  state = FMT;
	  //check if this lines is also the last
	}else if (*(line_end-1) == '\0'){
	  string last_value(media_line, int(line_end-media_line)-1);
	  payload.type = m.type;
	  str2i(last_value, payload_type);
	  payload.payload_type = payload_type;
	  m.payloads.push_back(payload);
	  parsing = 0;
	  //if not
	}else{
	  string last_value(media_line, int(line_end-media_line)-1);
	  payload.type = m.type;
	  str2i(last_value, payload_type);
	  payload.payload_type = payload_type;
	  m.payloads.push_back(payload);
	  parsing=0;
	}
	break;
      }
    }
  }
  sdp_msg->media.push_back(m);

  DBG("parse_sdp_line_ex: parse_sdp_media: done parsing media description \n");
  return;
}

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
    DBG("got session attribute '%.*s\n", attr_end-s+1, s);
  } else {
    // value attribute
    sdp_msg->attributes.push_back(SdpAttribute(string(s, col-s-1),
					       string(col, attr_end-col+1)));
    DBG("got session attribute '%.*s:%.*s'\n", col-s-1, s, attr_end-col+1, col);

  }

}

static void parse_sdp_attr(AmSdp* sdp_msg, char* s)
{
 
  //  DBG("parse_sdp_line_ex: parse_sdp_attr.......\n");
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

  if(contains(attr_line, line_end, ':')){
    next = parse_until(attr_line, ':');
    string attr(attr_line, int(next-attr_line)-1);
    attr_line = next;
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
	    }else{
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
	    }else if (*(line_end-1) == '\0') {
	      string clk_rate(attr_line, int(line_end-attr_line)-1);
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
      
      DBG("found media attr 'rtpmap' type '%d'\n", payload_type);
      
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

      DBG("found media attr 'fmtp' for payload '%d': '%s'\n", 
	  payload_type, params.c_str());

      vector<SdpPayload>::iterator pl_it;
      
      for(pl_it=media.payloads.begin();
	   (pl_it != media.payloads.end())
	     && (pl_it->payload_type != int(payload_type));
	   pl_it++);

      if(pl_it != media.payloads.end())
	pl_it->sdp_format_parameters = params;

    } else if (attr == "direction") {
	next = parse_until(attr_line, '\r');
	if(next < line_end){
	    string value(attr_line, int(next-attr_line)-1);
	    if (value == "active") {
		media.dir=SdpMedia::DirActive;
		DBG("found media attr 'direction' value '%s'\n",
		    (char*)value.c_str());
	    } else if (value == "passive") {
		media.dir=SdpMedia::DirPassive;
		DBG("found media attr 'direction' value '%s'\n",
		    (char*)value.c_str());
	    } else if (attr == "both") {
		media.dir=SdpMedia::DirBoth;
		DBG("found media attr 'direction' value '%s'\n",
		    (char*)value.c_str());
	    } else
		DBG("found media attr 'direction' with unknown value '%s'\n",
		    (char*)value.c_str());
	} else {
	    DBG("found media attr 'direction', but value is not"
		" followed by cr\n");
	}

    }else{
      attr_check(attr);
      next = parse_until(attr_line, '\r');
      if(next < line_end){
	  string value(attr_line, int(next-attr_line)-1);
	  DBG("found media attr '%s' value '%s'\n",
	      (char*)attr.c_str(), (char*)value.c_str());
	  media.attributes.push_back(SdpAttribute(attr, value));
      } else {
	  DBG("found media attr '%s', but value is not followed by cr\n",
	      (char *)attr.c_str());
      }
    }


  } else {
      next = parse_until(attr_line, '\r');
      if(next < line_end){
	  string attr(attr_line, int(next-attr_line)-1);
	  attr_check(attr);
	  DBG("found media attr '%s'\n", (char*)attr.c_str());
	  media.attributes.push_back(SdpAttribute(attr));
      } else {
	  DBG("found media attr line '%s', which is not followed by cr\n",
	      attr_line);
      }
  }

  return;
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

  DBG("parse_sdp_line_ex: parse_sdp_origin: parsing sdp origin\n");

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
	    string unicast_addr(origin_line, int(line_end-origin_line)-1);
	  }else{
	    DBG("parse_sdp_origin: 'o=' contains more values than allowed; these values will be ignored\n");  
	    string unicast_addr(origin_line, int(next-origin_line)-1);
	  }
	  parsing = 0;
	  break;
	}
      }
  }
  
  sdp_msg->origin = origin;

  DBG("parse_sdp_line_ex: parse_sdp_origin: done parsing sdp origin\n");
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
    *line++;
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
  else
    {
    DBG("sdp_parse_attr: Unknown attribute name used:%s, plz see RFC4566\n", 
	(char*)attr.c_str());
    return false;
    }
}
