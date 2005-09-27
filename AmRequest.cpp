/*
 * $Id: AmRequest.cpp,v 1.63.2.5 2005/08/31 13:54:29 rco Exp $
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

#include "AmRequest.h"
#include "AmSession.h"
#include "AmUtils.h"
#include "AmConfig.h"
#include "AmServer.h"
#include "AmApi.h"
#include "SerClient.h"
#include "log.h"

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>


#define SIP_PREFIX     "sip:"
#define SIP_PREFIX_LEN (sizeof("sip:")-1)

#if 0
static int parse_uri(const string& uri, string& prefix, string& user, 
		     string& domain,string& port,string& params)
{
    string::size_type pos1,pos2;
    pos1=0;
    pos2=string::npos;
    
    if(uri.empty()){
	ERROR("passed empty uri to parse_uri.\n");
	return -1;
    }

    if(uri.substr(pos1,SIP_PREFIX_LEN) != SIP_PREFIX){
	ERROR("passed non sip uri to parse_uri ('%s')\n",uri.c_str());
	return -1;
    }
    pos1 += SIP_PREFIX_LEN;

    // [RFC 3261]
    //   userinfo =  ( user / telephone-subscriber ) [ ":" password ] "@"
    for( pos2 = pos1;; pos2++ ){

	if(pos2 >= uri.length()){
	    ERROR("parse_uri: missing '@'\n");
	    return -1;
	}

	if(uri[pos2] == '@'){
	    user = uri.substr(pos1,pos2-pos1);
	    pos1 = pos2+1;
	    break;
	}

	if(AmConfig::PrefixSep && (uri[pos2] == AmConfig::PrefixSep)){
	    prefix = uri.substr(pos1,pos2-pos1);
	    pos1 = pos2+1;
	}
    }

    // [RFC 3261]
    //   hostport =  host [ ":" port ]
    bool is_host=true;
    for( ++pos2; pos2 < uri.length(); ++pos2 ){
	switch(uri[pos2]){
	case ';':
	case '?':
	    break;
	case ':':
	    is_host = false;
	    domain = uri.substr(pos1,pos2-pos1);
	    pos1 = pos2+1;
	default:
	    continue;
	}
	break;
    }
    if(pos2>pos1){
	if(is_host)
	    domain = uri.substr(pos1,pos2-pos1);
	else
	    port = uri.substr(pos1,pos2-pos1);
	pos1 = pos2;
    }

    // parameters & headers (see RFC 3261)
    if(pos2 < uri.length())
	params = uri.substr(pos1);

    return 0;
}
#endif

AmRequest::AmRequest(const AmCmd& _cmd)
    : cmd(_cmd),replied(false),
      session_container(AmSessionContainer::instance())
{
}

int AmRequest::sendRequest(const string& method, 
			   unsigned int& code, 
			   string& reason) 
{
  string auth_dummy;
//   DBG("dummy sendrequest\n");
  return sendRequest(method, code, reason, auth_dummy);
}

int AmRequest::sendRequest(const string& method, 
			   unsigned int& code, 
			   string& reason,
			   string& auth_header,
			   const string& body, 
			   const string& content_type)
{
    string msg,ser_cmd;

    string route = cmd.route;
    string res_fifo = AmSession::getNewId();
    
    replied = false;

    if (method == "CANCEL") {
	ser_cmd = "t_uac_cancel";

	msg = cmd.callid + "\n"
	    + int2str(cmd.cseq+1) + "\n\n";
    } else {
	ser_cmd = "t_uac_dlg";

	msg = method + "\n"
	    + cmd.from_uri + "\n";
    
	if(cmd.next_hop.empty())
	    msg += ".";
	else
	    msg += cmd.next_hop;
	msg += "\n";
    
	msg += "From: " + cmd.to;
	if(cmd.to_tag.length()) msg += ";tag=" + cmd.to_tag;
	msg += "\n";
	
	msg += "To: " + cmd.from;
	if(cmd.from_tag.length()) msg += ";tag=" + cmd.from_tag;
	msg += "\n";
	
	msg += "CSeq: " + int2str(++cmd.cseq) + " " + method + "\n"
	    + "Call-ID: " + cmd.callid + "\n";
	
	msg += getContactHdr();

	// todo: do not send xtra recved headers
	if(cmd.hdrs.length()){
	    msg += cmd.hdrs;
	    if(cmd.hdrs[cmd.hdrs.length()-1] != '\n')
		msg += "\n";
	}
	
	if(!route.empty() && (route.find("Route: ")!=string::npos))
	    route = route.substr(7/*sizeof("Route: ")*/);
	
	while(!route.empty()){
	    
	    string::size_type comma_pos;
	    
	    comma_pos = route.find(',');
	    msg += "Route: " + route.substr(0,comma_pos) + "\n";
	    
	    if(comma_pos != string::npos)
		route = route.substr(comma_pos+1);
	    else
		route = "";
	}
	
	if(!body.empty())
	    msg += "Content-Type: " + content_type + "\n";
	
	msg += ".\n" // EoH
	    + body + ".\n\n";
    }  

    int ret = send(ser_cmd,msg,code,reason, auth_header);
    replied = (ret == 0);
    
    return ret;
}
int AmRequest::send(const string& ser_cmd, const string& msg, 
		    unsigned int& code, string& reason)
{
  string auth_hdr_dummy;
  return send(ser_cmd, msg, code, reason, auth_hdr_dummy);
}


int AmRequest::send(const string& ser_cmd, const string& msg, 
		    unsigned int& code, string& reason, string& auth_header)
{
    SerClient* client = SerClient::getInstance();
    int id = client->open();
    if(id == -1)
	return -1;
	
    char* buffer=0;
    if(client->send(id,ser_cmd,msg,SER_SIPREQ_TIMEOUT) == -1){
	ERROR("while sending request to Ser\n");
	goto error;
    }

    buffer = client->getResponseBuffer(id);
    if(!buffer){
	ERROR("got no response from Ser\n");
	goto error;
    }

    if(getReturnCode(buffer,code,reason,auth_header) == -1){
	ERROR("while parsing Ser's response\n");
    }
    
    client->close(id);
    return 0;

 error:
    client->close(id);
    return -1;
}

void AmRequest::error(int code, const string& reason)
{
    if(replied)
	bye();
    else
	reply(code,reason);
}

int AmRequest::bye()
{
    unsigned int code;
    string reason;

    int ret = sendRequest("BYE",code,reason);
    if(!ret && ((code < 200) || (code >= 300))){
	ERROR("AmRequest::bye: %i %s\n",code,reason.c_str());
	return -1;
    }

    return ret;
}

string AmRequest::getContactHdr()
{
    string msg;

    if(cmd.r_uri.empty()){
	msg += "Contact: <sip:" + cmd.cmd;
	if(AmConfig::PrefixSep)
	    msg += AmConfig::PrefixSep;
	msg += "@!!>\n";
    }
    else {

      string user = cmd.user;
      if(AmConfig::PrefixSep){
	string::size_type sep_pos = cmd.user.find(AmConfig::PrefixSep);
	if(sep_pos != string::npos)
	  user = cmd.user.substr(sep_pos+1);
      }

      msg += "Contact: <sip:";
	
      if(AmConfig::PrefixSep)
	msg += cmd.cmd + AmConfig::PrefixSep;

#ifdef SUPPORT_IPV6
      if(cmd.dstip.find('.') != string::npos)
	  msg += user + "@" + cmd.dstip;
      else
	  msg += user + "@[" + cmd.dstip + "]";
#else
      msg += user + "@" + cmd.dstip;
#endif
      if(!cmd.port.empty())
	msg += ":" + cmd.port;
      msg += ">\n";
    }

    return msg;
}

void AmRequestUAS::accept(int localport)
{
    string sdp_buf;
    
    sdp.genResponse(cmd.dstip,localport,sdp_buf);
    if(reply(200,"OK",sdp_buf)!=0)
	throw AmSession::Exception(500,"could not send response.");
}

int AmRequestUAS::reply(int code, const string& reason, const string& body)
{
    string code_str = int2str(code);
    string msg = code_str + "\n" + reason + "\n" + 
	cmd.key + "\n" + cmd.to_tag + "\n";

    msg += getContactHdr();

    if(!body.empty())
	msg += "Content-Type: application/sdp\n";

    msg += ".\n";

    if(!body.empty())
	msg += body;

    msg += ".\n\n";

    unsigned int res_code;
    string res_reason;
    int ret = send("t_reply",msg,res_code,res_reason);
    if(!ret && ((res_code < 200) || (res_code >= 300))){
	ERROR("AmRequestUAS::reply: %i %s\n",res_code,res_reason.c_str());
	return -1;
    }

    if(code >= 200)
	replied = true;

    return ret;
}

int AmRequest::getReturnCode(char* reply_buf,
			     unsigned int& code,
			     string& reason,
			     string& auth_hdr) 
{

    char*  msg_c = reply_buf;
    char   res_code_str[4] = {'\0'};
    string tmp_str;

    try {
	msg_get_param(msg_c,tmp_str);
	
	DBG("response from Ser: %s\n",tmp_str.c_str());
	if( parse_return_code(tmp_str.c_str(),res_code_str,code,reason) == -1 ){
	    ERROR("while parsing return code from Ser.\n");
	    goto error;
	}

	if(code >= 200 && code < 300){

	    AmCmd n_cmd;
	    char  body[MSG_BODY_SIZE];
	    char  hdrs[MSG_BODY_SIZE];
	
	    /* Parse complete response:
	     *
	     *   [next_request_uri->cmd.from_uri]CRLF
	     *   [next_hop->cmd.next_hop]CRLF
	     *   [route->cmd.route]CRLF
	     *   ([headers->n_cmd.hdrs]CRLF)*
	     *   CRLF
	     *   ([body->body]CRLF)*
	     */
	
	    msg_get_param(msg_c,n_cmd.from_uri);
	    if(!n_cmd.from_uri.empty()) cmd.from_uri = n_cmd.from_uri;

	    msg_get_param(msg_c,n_cmd.next_hop);
	    if(!n_cmd.next_hop.empty()) cmd.next_hop = n_cmd.next_hop;

	    msg_get_param(msg_c,n_cmd.route);
	    if(!n_cmd.route.empty()) cmd.route = n_cmd.route;

	    int ret = msg_get_lines(msg_c,hdrs,MSG_BODY_SIZE);
	    if(ret > 0){
		n_cmd.hdrs += string(hdrs);
		DBG("headers : <%s>\n",hdrs);
	    }
	    
	    if(!n_cmd.hdrs.empty()){

		cmd.from = n_cmd.getHeader("To");
		DBG("found 'To: %s'\n",cmd.from.c_str());

		string::size_type p = cmd.from.find(";tag=");
		if(p != string::npos){
		    p += 5/*sizeof(";tag=")*/;
		    unsigned int p_end = p;
		    while(p_end < cmd.from.length()){
			if( cmd.from[p_end] == '>'
			    || cmd.from[p_end] == ';' )
			    break;
			p_end++;
		    }
		    cmd.from_tag = cmd.from.substr(p,p_end-p);
		    DBG("found To-tag: '%s'\n",cmd.from_tag.c_str());
		}

		cmd.callid = n_cmd.getHeader("Call-ID");

		ret = msg_get_lines(msg_c,body,MSG_BODY_SIZE);
	    }

	    if(body[0]!='\0'){
		sdp.setBody(body);
	    }
	}  else if ((code == 407) || (code == 401)) {
	    AmCmd n_cmd;
	    char  hdrs[MSG_BODY_SIZE];
	    msg_get_param(msg_c,n_cmd.from_uri);
	    if(!n_cmd.from_uri.empty()) cmd.from_uri = n_cmd.from_uri;

	    msg_get_param(msg_c,n_cmd.next_hop);
	    if(!n_cmd.next_hop.empty()) cmd.next_hop = n_cmd.next_hop;

	    msg_get_param(msg_c,n_cmd.route);
	    if(!n_cmd.route.empty()) cmd.route = n_cmd.route;

	    int ret = msg_get_lines(msg_c,hdrs,MSG_BODY_SIZE);
	    if(ret > 0){
		n_cmd.hdrs += string(hdrs);
		DBG("auth required - headers : <%s>\n",hdrs);
	    }
	    
	    if(!n_cmd.hdrs.empty()){
	      auth_hdr = (code==407) ? n_cmd.getHeader("Proxy-Authenticate") : 
		n_cmd.getHeader("WWW-Authenticate");
	      DBG("found auth_hdr '%s'\n", auth_hdr.c_str());
	    }
	}
    }
    catch(const string& err){
	ERROR("while receiving return code from Ser: %s\n",err.c_str());
	goto error;
    }

    return 0;

 error:
    return -1;
}

AmRequestUAS::AmRequestUAS(const AmCmd& _cmd, char* body_buf)
    : AmRequest(_cmd) 
{
    if(body_buf){
	m_body = body_buf;
    }
}

AmRequest* AmRequestUAS::duplicate()
{
    return new AmRequestUAS(*this);
}

void AmRequestUAC::add_credential(const UACAuthCredential& cred) {
  credentials.push_back(cred);
}

void AmRequestUAS::execute()
{
    string hash_str = cmd.callid;
    if(cmd.from_tag.length())
	hash_str += cmd.from_tag;

    if(cmd.method == "INVITE") {

	sdp.setBody(m_body.c_str());
	session_container->startSession(hash_str,cmd.to_tag,this);
    }
    else if(cmd.method == "BYE" || cmd.method == "CANCEL"){
	
// 	if(session_container->sadSession(hash_str,cmd.to_tag))
	if(!session_container->postEvent( hash_str,cmd.to_tag,
					  new AmSessionEvent(AmSessionEvent::Bye,*this)))
	    reply(200,"OK");
	else 
	    reply(481,"Call/Transaction Does Not Exist");
    }
    else if(cmd.method == "REFER"){
	
	if(cmd.to_tag.empty()){
	    // Out-of-dialog REFER
	    string refer_to = uri_from_name_addr(cmd.getHeader("Refer-To"));
	    AmCmd dial_cmd =
		AmRequestUAC::dialout(cmd.user,
				      cmd.cmd,
				      refer_to,
				      cmd.r_uri);

	    cmd.to_tag = AmSession::getNewId();
	    reply(202,"Accepted");
	}
	else if(!session_container->postEvent( hash_str,cmd.to_tag,
					       new AmSessionEvent(AmSessionEvent::Refer,*this) 
					       ) ) {
	    reply(481,"Call/Transaction Does Not Exist");
	}
    }
    else if(cmd.method == "NOTIFY"){
	
	if(!session_container->postEvent( hash_str,cmd.to_tag,
					  new AmNotifySessionEvent(/*AmSessionEvent::Notify,*/*this) 
					  ) ) {
	    
	    reply(481,"Call/Transaction Does Not Exist");
	}
    }
    else if (cmd.method == "INFO")
    {
        AmSession *s = session_container->getSession(hash_str, cmd.to_tag);
        if (s)
        {
            s->postDtmfEvent(new AmSipDtmfEvent(m_body));
            reply(200, "OK");
        }
        else
        {
            reply(481,"Call/Transaction Does Not Exist");
        }
    }
    else
	reply(500,"Method not supported");
}

AmRequest* AmRequestUAC::duplicate()
{
    return new AmRequestUAC(*this);
}

void AmRequestUAC::execute()
{
    string hash_str = cmd.callid;
    if(cmd.to_tag.length())
	hash_str += cmd.to_tag;

    try {
	unsigned int code;
	string       reason;

	if(cmd.method == "INVITE"){

	    AmSession* session = session_container->createSession(cmd.cmd,this);
	    if(session){

		try {
		    if(cmd.dstip.empty())
			throw string("AmCmd::dstip is empty.");
		    
		    string body;
		    session->rtp_str.setLocalIP(cmd.dstip);
		    sdp.genRequest(cmd.dstip,session->rtp_str.getLocalPort(),body);
		    

		    string auth_hdr;
		    if(sendRequest(cmd.method,code,reason,
				   auth_hdr, body,content_type) != 0) 
			throw string("while sending request.");
		    
		    // try again with auth while response is 407 or 401
		    if (auth_hdr.length()) {
		      vector<UACAuthCredential>::iterator it = credentials.begin();
		      while (((code == 401) || (code == 407)) && (it != credentials.end())) {
			if (!it->do_auth(cmd, code, auth_hdr)) {
			  DBG("do_auth failed.\n");
			  it++;
			  continue;
			}
			DBG("trying again to send request with auth...\n");
			if(sendRequest(cmd.method,code,reason,
				       auth_hdr, body,content_type) != 0) 
			  throw string("while sending request.");
			it++;
		      }
		    }

		    notify_uac_req_status(code, reason);

		    if( (code < 200) || (code >= 300) ){
			session->getDialogState()->onError(code,reason);
			AmThreadWatcher::instance()->add(session);
			return;
		    }

		    if(cmd.from_tag.empty())
			throw string("no remote user tag.");
		    
		    if(cmd.callid.empty())
			throw string("no callid.");

		    if(cmd.to_tag.empty())
			throw string("no local user tag.");

		    if(!m_body.empty())
			sdp.setBody(m_body.c_str());

		    session->req.reset(this->duplicate());
		    session_container->addSession(cmd.callid+cmd.from_tag,
						  cmd.to_tag,session);
		    session->start();
		    
		}
		catch(...){
		    AmThreadWatcher::instance()->add(session);
		    throw;
		}
	    }
	} if(cmd.method == "NOTIFY") { // this one goes with body
	    DBG("send request...\n");
	    string auth_hdr;
	    sendRequest(cmd.method, code, reason, auth_hdr, m_body, content_type);

	    notify_uac_req_status(code, reason);
	    
 	    if( (code < 200) || (code >= 300) )  
 		throw AmSession::Exception(code,reason);
	}
	else {
	    string auth_hdr;
	    sendRequest(cmd.method, code, reason, auth_hdr);
	    notify_uac_req_status(code, reason);

	    // try again with auth while response is 407 or 401
	    if (auth_hdr.length()) {
	      vector<UACAuthCredential>::iterator it = credentials.begin();
	      while (((code == 401) || (code == 407)) && (it != credentials.end())) {
		if (!it->do_auth(cmd, code, auth_hdr)) {
		  DBG("do_auth failed.\n");
		  continue;
		}
		DBG("trying again to send request with auth...\n");
		if(sendRequest(cmd.method,code,reason,
			       auth_hdr) != 0) 
		  throw string("while sending request.");
		notify_uac_req_status(code, reason);
		it++;
	      }
	    }

	    if( (code < 200) || (code >= 300) ) 
		throw AmSession::Exception(code,reason);
	}
    }
    catch(const AmSession::Exception& e){
 	ERROR("%i %s\n",e.code,e.reason.c_str());
    }
    catch(const string& err){
	ERROR("AmRequestUAC::execute: %s\n",err.c_str());
    }
    catch(...){
	ERROR("unexpected exception\n");
    }
}

void AmRequestUAC::notify_uac_req_status(unsigned int code, string& reason) {

  if (status_subscriber_dlg_hash.length() && 
	status_subscriber_dlg_key.length()) {
	AmSessionContainer::instance()->postGenericEvent(
	    status_subscriber_dlg_hash, 
	    status_subscriber_dlg_key, 
	    new AmRequestUACStatusEvent( ((code < 200) || (code >= 300)) ?
					 AmRequestUACStatusEvent::Error : 
					 AmRequestUACStatusEvent::Accepted,
					 cmd, code, reason));
    }
}

AmCmd AmRequestUAC::dialout(const string& user,
			    const string& app_name,
			    const string& uri, 
			    const string& from,
			    string dialout_status_subscriber_hash, 
			    string dialout_status_subscriber_key)
{
    AmCmd cmd;

    cmd.cmd      = app_name;
    cmd.user     = user;
    cmd.method   = "INVITE";
    cmd.dstip    = AmConfig::LocalIP;
    cmd.from_uri = uri;
    cmd.from     = cmd.from_uri;
    cmd.to       = from;
    cmd.to_tag   = AmSession::getNewId();
    cmd.cseq     = 10;
    cmd.callid   = AmSession::getNewId() + "@" + AmConfig::LocalIP;

    AmASyncRequestUAC* req = new AmASyncRequestUAC(cmd, dialout_status_subscriber_hash, 
						   dialout_status_subscriber_key);
    req->start();
    AmThreadWatcher::instance()->add(req);
    
    return cmd;
}


AmCmd AmRequestUAC::dialoutEx(const string& user,
			      const string& app_name,
			      const string& uri, 
			      const string& from,
			      const string& next_hop,
			      const UACAuthCredential cred,
			      string dialout_status_subscriber_hash, 
			      string dialout_status_subscriber_key)
{
    AmCmd cmd;

    cmd.cmd      = app_name;
    cmd.user     = user;
    cmd.method   = "INVITE";
    cmd.dstip    = AmConfig::LocalIP;
    cmd.from_uri = uri;
    cmd.from     = cmd.from_uri;
    cmd.to       = from;
    cmd.to_tag   = AmSession::getNewId();
    cmd.cseq     = 10;
    cmd.callid   = AmSession::getNewId() + "@" + AmConfig::LocalIP;
    cmd.next_hop = next_hop;

    AmASyncRequestUAC* req = new AmASyncRequestUAC(cmd, dialout_status_subscriber_hash, 
						   dialout_status_subscriber_key);
    req->add_credential(cred);
    req->start();
    AmThreadWatcher::instance()->add(req);
    
    return cmd;
}

AmCmd AmRequestUAC::registerUAC(const std::string& app_name,
				const std::string& uri,
				const std::string& to,
				const std::string& from,
				const std::string& user,
				const std::string& next_hop,
				const unsigned int expires,
				const UACAuthCredential cred,
				std::string dialout_status_subscriber_hash, 
				std::string dialout_status_subscriber_key) {
    AmCmd cmd;

    cmd.cmd      = app_name;
    cmd.user     = user;
    cmd.method   = "REGISTER";
    cmd.dstip    = AmConfig::LocalIP;
    cmd.from_uri = uri;
    cmd.from     = to; 
    cmd.to       = from;
    cmd.to_tag   = AmSession::getNewId();
    cmd.cseq     = 10;
    cmd.callid   = AmSession::getNewId() + "@" + AmConfig::LocalIP;
    cmd.next_hop = next_hop;
    cmd.r_uri = "sip:"+user+"@"+AmConfig::LocalIP;
    
    cmd.addHeader("Expires: "+int2str(expires)+"\n");

    DBG("registering with user %s\n", cmd.user.c_str());
    AmASyncRequestUAC* req = new AmASyncRequestUAC(cmd, dialout_status_subscriber_hash, 
						   dialout_status_subscriber_key);
    req->add_credential(cred);
    req->start();
    AmThreadWatcher::instance()->add(req);
    
    return cmd;
}

AmCmd AmRequestUAC::refer(const AmCmd& cmd,
			  const string& refer_to,
			  string refer_status_subscriber_hash, 
			  string refer_status_subscriber_key)
{
    AmCmd m_cmd(cmd);

    m_cmd.method   = "REFER";
    m_cmd.hdrs     = "Refer-To: " + refer_to + "\n";
    m_cmd.hdrs     += "Referred-By: " + m_cmd.from; // make cisco phone happy. (should be to or r_uri)


    AmASyncRequestUAC* req = new AmASyncRequestUAC(m_cmd, refer_status_subscriber_hash, 
						   refer_status_subscriber_key);
    req->start();
    AmThreadWatcher::instance()->add(req);

    return m_cmd;
}

void AmRequestUAC::cancel(const AmCmd& cmd)
{
    AmCmd m_cmd(cmd);
    m_cmd.method = "CANCEL";

    AmASyncRequestUAC* req = new AmASyncRequestUAC(m_cmd);
    req->start();
    AmThreadWatcher::instance()->add(req);
}

AmASyncRequestUAC::AmASyncRequestUAC(const AmCmd& _cmd, 
				     string status_subscriber_dlg_hash, 
				     string status_subscriber_dlg_key)
    : AmRequestUAC(_cmd, status_subscriber_dlg_hash, 
		   status_subscriber_dlg_key)
{
}

void AmASyncRequestUAC::run()
{
    execute();
}

void AmASyncRequestUAC::on_stop()
{
}

AmRequestUACStatusEvent::AmRequestUACStatusEvent(EventType event_type, 
						 const AmRequestUAC& request,
			int code_, string reason_)
    : AmEvent(int(event_type)), request(request),
      code(code_), reason(reason_) 
{
}
