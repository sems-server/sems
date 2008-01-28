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

#include <stdio.h>
#include <fcntl.h>
#include <assert.h>

#include "AmConfig.h"
#include "AmSdp.h"
#include "AmUtils.h"
#include "AmPlugIn.h"
#include "AmSession.h"

#include "amci/amci.h"
#include "log.h"

// Not on Solaris!
#if !defined (__SVR4) && !defined (__sun)
#include "strings.h"
#endif

inline char* get_next_line(char* s);
inline bool parse_string_tok(char*& s, string& res, char sep_char = ' ');
inline bool parse_type(char*& s, int& v, char** ref, const char* name);

static bool parse_sdp_connection(AmSdp* sdp_msg, char*& s, char*&);
static bool parse_sdp_media(AmSdp* sdp_msg, char*& s, char*&);
static bool parse_sdp_attribute(AmSdp* sdp_msg, char*& s, char*&);


// inline functions

inline string net_t_2_str(int nt)
{
  switch(nt){
  case NT_IN: return "IN";
  default:    return "<unknown network type>";
  }
}

inline string addr_t_2_str(int at)
{
  switch(at){
  case AT_V4: return "IP4";
  case AT_V6: return "IP6";
  default:    return "<unknown address type>";
  }
}

inline string media_t_2_str(int mt)
{
  switch(mt){
  case MT_AUDIO:       return "audio";
  case MT_VIDEO:       return "video";
  case MT_APPLICATION: return "application";
  case MT_DATA:        return "data";
  default:             return "<unknown media type>";
  }
}

inline string transport_p_2_str(int tp)
{
  switch(tp){
  case TP_RTPAVP: return "RTP/AVP";
  case TP_UDP:    return "udp";
  default:        return "<unknown transport protocol>";
  }
}

bool SdpPayload::operator == (int r) 
{ 
  DBG("pl == r: payload_type = %i; r = %i\n",payload_type, r);
  return payload_type == r; 
}


// WARNING:
//
// If you call this function from a handler function, 
// do not forget to set 'next_line' to the proper value
// (else, it could think EOT has been reached !)
//
// Generally: call this function if you know what your doing !
//            Else call 'parse_sdp_line'
//
inline bool parse_sdp_line_ex( AmSdp* sdp_msg, char*& s, char discr,
			       bool optional, 
			       bool (*parse_func)(AmSdp*,char*&,char*&),
			       bool only_one, char*& next_line);

// WARNING:
//
// Do not touch the handler's third parameter until you known what you are doing.
//
inline bool parse_sdp_line( AmSdp* sdp_msg, char*& s, char discr,
			    bool optional, bool (*parse_func)(AmSdp*,char*&,char*&),
			    bool only_one = true );

//
// class AmSdp: Methods
//
AmSdp::AmSdp() 
  : remote_active(false),
    telephone_event_pt(NULL)

{ 
}

AmSdp::AmSdp(const AmSdp& p_sdp_msg)
  : version(p_sdp_msg.version),     
    origin(p_sdp_msg.origin),      
    sessionName(p_sdp_msg.sessionName), 
    conn(p_sdp_msg.conn),
    media(p_sdp_msg.media),
    telephone_event_pt(NULL),
    remote_active(false)
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

  bool ret = 
    parse_sdp_line(this,s,'v',false,NULL) ||
    parse_sdp_line(this,s,'o',false,NULL) ||
    parse_sdp_line(this,s,'s',false,NULL) ||
    parse_sdp_line(this,s,'i',true,NULL) ||
    parse_sdp_line(this,s,'u',true,NULL) ||
    parse_sdp_line(this,s,'e',true,NULL,false) ||
    parse_sdp_line(this,s,'p',true,NULL,false) ||
    parse_sdp_line(this,s,'c',true,parse_sdp_connection) ||
    parse_sdp_line(this,s,'b',true,NULL,false) ||
    parse_sdp_line(this,s,'t',true,NULL,false) ||
    parse_sdp_line(this,s,'k',true,NULL) ||
    parse_sdp_line(this,s,'a',true,NULL,false) ||
    parse_sdp_line(this,s,'m',false,parse_sdp_media,false);

  if(!ret && conn.address.empty()){
    for(vector<SdpMedia>::iterator it = media.begin();
	!ret && (it != media.end()); ++it)
      ret = it->conn.address.empty();

    if(ret){
      ERROR("A connection field must be present in every\n");
      ERROR("media description or at the session level.\n");
    }
  }

  telephone_event_pt = findPayload("telephone-event");

  return ret;
}


int AmSdp::genResponse(const string& localip, int localport, 
		       string& out_buf, bool single_codec)
{
  string l_ip = "IP4 " + localip;

#ifdef SUPPORT_IPV6
  if(localip.find('.') == string::npos)
    l_ip = "IP6 " + localip;
#endif

  out_buf = 
    "v=0\r\n"
    "o=- 0 0 IN " + l_ip + "\r\n"
    "s=session\r\n";
  if (!uri.empty())
    out_buf+="u="+uri+"\r\n";
  out_buf+=
    "c=IN " + l_ip + "\r\n"
    "t=0 0\r\n"
    "m=audio " + int2str(localport) + " RTP/AVP";

  string payloads;
  string options;

  for(vector<SdpPayload*>::iterator it = sup_pl.begin();
      it !=  sup_pl.end(); ++it){

    payloads += " " + int2str((*it)->payload_type);
    options += "a=rtpmap:" + int2str((*it)->payload_type) + " " 
      + (*it)->encoding_name + "/" + int2str((*it)->clock_rate) + "\r\n";

    if ((*it)->sdp_format_parameters.size()) { 
      // return format parameters as sent in the invite
      //  (we have initialized our codec with those)
      options += "a=fmtp:" + int2str((*it)->payload_type) + " " 
	+ (*it)->sdp_format_parameters + "\r\n";
    }
    if (single_codec) break;
  }

  if (hasTelephoneEvent())
    payloads += " " + int2str(telephone_event_pt->payload_type);

  out_buf += payloads + "\r\n"
    + options;

  if (hasTelephoneEvent())
    {
      out_buf += "a=rtpmap:" + int2str(telephone_event_pt->payload_type) + " " + 
	telephone_event_pt->encoding_name + "/" +
	int2str(telephone_event_pt->clock_rate) + "\r\n"
	"a=fmtp:" + int2str(telephone_event_pt->payload_type) + " 0-15\r\n";
    }
    
  if(remote_active /* dir == SdpMedia::DirActive */)
    out_buf += "a=direction:passive\r\n";

  return 0;
}

int AmSdp::genRequest(const string& localip,int localport, string& out_buf)
{
  AmPlugIn* plugin = AmPlugIn::instance();
  const std::map<int,amci_payload_t*>& payloads = plugin->getPayloads();
  const std::map<int,int>& payload_order = plugin->getPayloadOrder();

  if(payloads.empty()){
    ERROR("no payload plugin loaded.\n");
    return -1;
  }

  string l_ip = "IP4 " + localip;

#ifdef SUPPORT_IPV6
  if(localip.find('.') == string::npos)
    l_ip = "IP6 " + localip;
#endif

  out_buf = 
    "v=0\r\n"
    "o=- 0 0 IN " + l_ip + "\r\n"
    "s=session\r\n";
  if (!uri.empty())
    out_buf+="u="+uri+"\r\n";
  out_buf+=
    "c=IN " + l_ip + "\r\n"
    "t=0 0\r\n"
    "m=audio " + int2str(localport) + " RTP/AVP ";
    
  std::map<int,int>::const_iterator it = payload_order.begin();
  out_buf += int2str((it++)->second);

  for(; it != payload_order.end(); ++it)
      out_buf += string(" ") + int2str(it->second);

  out_buf += "\r\n";

  for (it = payload_order.begin(); it != payload_order.end(); ++it) {
      std::map<int,amci_payload_t*>::const_iterator it2 = payloads.find(it->second);
      if (it2 != payloads.end()) {
	  out_buf += "a=rtpmap:" + int2str(it2->first) 
	      + " " + string(it2->second->name) 
	      + "/" + int2str(it2->second->sample_rate) 
	      + "\r\n";
      } else {
	  ERROR("Payload %d was not found in payloads map!\n", it->second);
	  return -1;
      }
  }

  return 0;
}

const vector<SdpPayload *>& AmSdp::getCompatiblePayloads(int media_type, string& addr, int& port)
{
  vector<SdpMedia>::iterator   m_it;
  SdpPayload *payload;
  sup_pl.clear();

  AmPlugIn* pi = AmPlugIn::instance();

  for( m_it = media.begin(); m_it != media.end(); ++m_it ){

    if( (media_type != m_it->type) )
      continue;

    vector<SdpPayload>::iterator it = m_it->payloads.begin();
    for(; it != m_it->payloads.end(); ++it ) {

      amci_payload_t* a_pl = NULL;
      if(it->payload_type < DYNAMIC_PAYLOAD_TYPE_START){
	// try static payloads
	a_pl = pi->payload(it->payload_type);
      }

      if( a_pl ) {
		    
	payload = &(*it);
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

	int int_pt = getDynPayload(it->encoding_name,
				   it->clock_rate);
		
	if(int_pt != -1){
		    
	  payload = &(*it);
	  payload->int_pt = int_pt;
	  sup_pl.push_back(payload);
	}
      }
    }
    if (sup_pl.size() > 0)
    {
      if (m_it->conn.address.empty())
      {
	DBG("using global address: %s\n",conn.address.c_str());
	addr = conn.address;
      }
      else {
	DBG("using media specific address: %s\n",m_it->conn.address.c_str());
	addr = m_it->conn.address;
      }
      
      if(m_it->dir == SdpMedia::DirActive)
	remote_active = true;
      
      port = (int)m_it->port;
    }
    break;
  }
  return sup_pl;
}

bool AmSdp::hasTelephoneEvent()
{
  return telephone_event_pt != NULL;
}

int AmSdp::getDynPayload(const string& name, int rate)
{
  AmPlugIn* pi = AmPlugIn::instance();
  const std::map<int, amci_payload_t*>& ref_payloads = pi->getPayloads();

  for(std::map<int, amci_payload_t*>::const_iterator pl_it = ref_payloads.begin();
      pl_it != ref_payloads.end(); ++pl_it)
    if( !strcasecmp(name.c_str(), pl_it->second->name)
	&&  (rate == pl_it->second->sample_rate) )
      return pl_it->first;
    
  return -1;
}

const SdpPayload *AmSdp::findPayload(const string& name)
{
  vector<SdpMedia>::iterator m_it;

  for (m_it = media.begin(); m_it != media.end(); ++m_it)
    {
      vector<SdpPayload>::iterator it = m_it->payloads.begin();
      for(; it != m_it->payloads.end(); ++it ) 
        {
	  if (it->encoding_name == name)
            {
	      return new SdpPayload(*it);
            }
        }
    }

  return NULL;
}



// enum { TP_NONE=0, TP_RTPAVP, TP_UDP };
char* transport_prot_lookup[] = { "RTP/AVP", "udp", 0 };

inline bool parse_transport_prot(char*& s, int& tp)
{ return parse_type(s,tp,transport_prot_lookup,"transport protocol"); }

// enum { MT_NONE=0, MT_AUDIO, MT_VIDEO, MT_APPLICATION, MT_DATA };
char* media_type_lookup[] = { "audio", "video", "application", "data", 0 };
#define parse_media_type(s,mt) parse_type(s,mt,media_type_lookup,"media type")

//inline bool parse_media_type(char*& s, int& mt)
//{ return parse_type(s,mt,media_type_lookup,"media type"); }

// enum { NT_OTHER=0, NT_IN };
char* net_type_lookup[] = { "IN", 0 };

inline bool parse_net_type(char*& s, int& network)
{ return parse_type(s,network,net_type_lookup,"net type"); }

// enum { AT_NONE=0, AT_V4, AT_V6 };
char* addr_type_lookup[] = { "IP4", "IP6", 0 };

inline bool parse_addr_type(char*& s, int& addr_t)
{ return parse_type(s,addr_t,addr_type_lookup,"address type"); }



inline char* get_next_line(char* s)
{
  char* next_line=s;

  // search for next line
  while( *next_line != '\0') {

    if(*next_line == 13){
      *next_line = '\0';
      next_line += 2;
      break;
    }
    else if(*next_line == 10){
      *(next_line++) = '\0';
      break;
    }
    next_line++;
  }
  return next_line;
}

// WARNING:
//
// If you call this function from a handler function, 
// do not forget to set 'next_line' to the proper value
// (else, it could think EOT has been reached !)
//
// Generally: call this function if you know what your doing !
//            Else call 'parse_sdp_line'
//
inline bool parse_sdp_line_ex( AmSdp* sdp_msg, char*& s, char discr,
			       bool optional, 
			       bool (*parse_func)(AmSdp*,char*&,char*&),
			       bool only_one, char*& next_line)
{
  while(true){

    if((*s == '\0') && !optional){
      ERROR("parse_sdp_line : unexpected end of text while looking for '%c'\n",discr);
      return true;
    }

    if(*s == discr) {

      if( *(++s) != '=' ){
	ERROR("parse_sdp_line : expected '=' but "
	      "<%c> found \n",*s);
	return true;
      }
      s++;

      next_line = get_next_line(s);
      bool ret=false;
      if(parse_func)
	ret = (*parse_func)(sdp_msg,s,next_line);
	    
      s = next_line;
      if(only_one || (*s != discr))
	return ret;

      continue;
    }
    else if(!optional){
      ERROR(" parse_sdp_line : parameter '%c=' was "
	    "not found\n",discr);
      return true;
    }

    // token is optional and has not been found.
    return false;
  }
}

// WARNING:
//
// Do not touch the handler's third parameter until you known what you are doing.
//
inline bool parse_sdp_line( AmSdp* sdp_msg, char*& s, char discr,
			    bool optional, bool (*parse_func)(AmSdp*,char*&,char*&),
			    bool only_one) 
{
  char* next_line=0;
  return parse_sdp_line_ex(sdp_msg,s,discr,optional,parse_func,only_one,next_line);
}

inline bool parse_string_tok(char*& s, string& res, char sep_char)
{
  for( ;(*s != '\0') && (*s == ' '); s++);
  char* begin = s;

  while(*s != '\0'){

    if(*s == sep_char){
      *(s++) = '\0';
      break;
    }
    s++;
  }

  res = begin;
  return res.empty();
}

inline bool parse_type(char*& s, int& v, char** ref, const char* name)
{
  string e;
  v=0;
  if(parse_string_tok(s,e)){
    ERROR(" parse_type : while parsing %s\n",name);
    return true;
  }

  char** cur = ref;
  while(*cur){
    if(e == *cur){
      v = cur-ref+1;
      break;
    }
    cur++;
  }
  if(!*cur){
    ERROR("unkown %s: <%s>\n",name,e.c_str());
    return true;
  }
  return false;
}

/*
  static bool parse_sdp_version(AmSdp* sdp_msg, char*& s, char*&)
  {
  return str2i(s,sdp_msg->version);
  }
*/

inline bool parse_sdp_connection_struct(SdpConnection& c, char*& s)
{
  return parse_net_type(s,c.network) ||
    parse_addr_type(s,c.addrType) ||
    parse_string_tok(s,c.address);
}

/*
  static bool parse_sdp_origin(AmSdp* sdp_msg, char*& s, char*&)
  {
  SdpOrigin& o = sdp_msg->origin;
  return parse_string_tok(s,o.user) || 
  str2i(s,o.sessId) ||
  str2i(s,o.sessV) ||
  parse_sdp_connection_struct(o.conn,s);
  parse_net_type(s,o.conn.network) ||
  parse_addr_type(s,o.conn.addrType) ||
  parse_string_tok(s,o.conn.address);
  }
*/

static bool parse_sdp_connection(AmSdp* sdp_msg, char*& s, char*&)
{
  return parse_sdp_connection_struct(sdp_msg->media.empty() ?
				     sdp_msg->conn :
				     sdp_msg->media.back().conn,
				     s);
}

inline bool parse_codec_list(char*& s, vector<SdpPayload>& payloads)
{
  unsigned int payload;

  while(*s != '\0'){

    if(!str2i(s,payload)){

      payloads.push_back(SdpPayload(payload));
    }
    else {
      ERROR("invalid payload number found in media line\n");
      return true;
    }
  }

  return false;
}

static bool parse_sdp_media(AmSdp* sdp_msg, char*& s, char*& next_line)
{
  SdpMedia m;
  m.dir = SdpMedia::DirBoth;

  char* old_s = s;
  bool ret = parse_media_type(s,m.type) || 
    str2i(s,m.port) || 
    parse_transport_prot(s,m.transport);

  if(ret){
    ERROR("while parsing 'm=%s' line.\n",old_s);
    return true;
  }

  if(!ret && (m.transport == TP_RTPAVP))
    ret = ret || parse_codec_list(s,m.payloads);

  sdp_msg->media.push_back(m);

  s = next_line;
  DBG("next_line=<%s>\n",next_line);
  ret = ret
    // Media title
    || parse_sdp_line(sdp_msg,s,'i',true,NULL)
    // connection information - optional if included at session-level
    || parse_sdp_line(sdp_msg,s,'c',true,parse_sdp_connection)
    // bandwidth information
    || parse_sdp_line(sdp_msg,s,'b',true,NULL,false)
    // encryption key
    || parse_sdp_line(sdp_msg,s,'k',true,NULL)
    // zero or more media attribute lines
    || parse_sdp_line(sdp_msg,s,'a',true,parse_sdp_attribute,false);

  if(ret){
    ERROR("while parsing media attributes.\n");
    return true;
  }

  next_line = get_next_line(s);
  DBG("ret=%i; next_line=<%s>\n",ret,next_line);

  return ret;
}

static bool parse_sdp_attribute(AmSdp* sdp_msg, char*& s, char*& next_line)
{
  DBG("parse_sdp_attribute: s=%s\n",s);
  if(sdp_msg->media.empty()){
    ERROR("While parsing media options: no actual media !\n");
    return true;
  }
	
  SdpMedia& media = sdp_msg->media.back();
    
  char* sep=0;
  for( sep=s; *sep!='\0' && *sep!=':'; sep++ );

  if( *sep == ':' ){

    // attribute definition: 'attribute:value'
    string attr_name(s,int(sep-s));
    char* old_s = s;
    s = sep + 1;

    if(attr_name == "rtpmap"){

      //fmt: "<payload type> <encoding name>/<clock rate>[/<encoding parameters>]"
      unsigned int payload_type=0,clock_rate=0;
      string encoding_name, params;

      bool ret = str2i(s,payload_type)
	|| parse_string_tok(s,encoding_name,'/')
	|| str2i(s,clock_rate,'/');

      if(ret){
	ERROR("while parsing 'a=%s'\n",old_s);
	return true;
      }

      parse_string_tok(s,params,'\0');
      DBG("sdp attribute: pt=%u; enc=%s; cr=%u\n",
	  payload_type,encoding_name.c_str(),clock_rate);

      vector<SdpPayload>::iterator pl_it;

      for( pl_it=media.payloads.begin();
	   (pl_it != media.payloads.end())
	     && (pl_it->payload_type != int(payload_type));
	   ++pl_it);

      if(pl_it != media.payloads.end()){
	*pl_it = SdpPayload( int(payload_type),
			     encoding_name,
			     int(clock_rate));
      }
      return ret;
    }
    else if (attr_name == "fmtp") {
      // fmt: "<payload type> parameters" (?)
      // z.b. a=fmtp:101 0-15
      unsigned int payload_type=0;
      string params;
      bool ret = str2i(s, payload_type) || parse_string_tok(s, params, '\0');

      vector<SdpPayload>::iterator pl_it;

      for( pl_it=media.payloads.begin();
	   (pl_it != media.payloads.end())
	     && (pl_it->payload_type != int(payload_type));
	   ++pl_it);

	  
      if(pl_it != media.payloads.end())
	pl_it->sdp_format_parameters = params;

      return ret;
    } 
    else if(attr_name == "direction"){
      if(!strncmp(s,"active",6/*sizeof("active")*/))
	media.dir = SdpMedia::DirActive;
      else if(!strncmp(s,"passive",7/*sizeof("passive")*/))
	media.dir = SdpMedia::DirPassive;
      else if(!strncmp(s,"both",4/*sizeof("both")*/))
	media.dir = SdpMedia::DirBoth;
      else
	DBG("unknown value for a=direction:%s",s);
    }
    else {
      DBG("unknown attribute definition '%s'\n",old_s);
    }
  }
  else {
    // flag: 'flag_name'
    DBG("flag definition is not yet supported (%s)\n",s);
  }

  return false;
}




