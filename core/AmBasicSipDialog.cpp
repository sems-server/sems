#include "AmBasicSipDialog.h"

#include "AmConfig.h"
#include "AmSipHeaders.h"
#include "SipCtrlInterface.h"
#include "AmSession.h"

#include "sip/parse_route.h"
#include "sip/parse_uri.h"
#include "sip/parse_next_hop.h"
#include "sip/msg_logger.h"
#include "sip/sip_parser.h"

const char* AmBasicSipDialog::status2str[AmBasicSipDialog::__max_Status] = {
  "Disconnected",
  "Trying",
  "Proceeding",
  "Cancelling",
  "Early",
  "Connected",
  "Disconnecting"
};

AmBasicSipDialog::AmBasicSipDialog(AmBasicSipEventHandler* h)
  : status(Disconnected),
    cseq(10),r_cseq_i(false),hdl(h),
    logger(0),
    outbound_proxy(AmConfig::OutboundProxy),
    force_outbound_proxy(AmConfig::ForceOutboundProxy),
    next_hop(AmConfig::NextHop),
    next_hop_1st_req(AmConfig::NextHop1stReq),
    outbound_interface(-1),
    nat_handling(false),
    usages(0)
{
  //assert(h);
}

AmBasicSipDialog::~AmBasicSipDialog()
{
  if (logger) dec_ref(logger);
  dump();
}

AmSipRequest* AmBasicSipDialog::getUACTrans(unsigned int t_cseq)
{
  TransMap::iterator it = uac_trans.find(t_cseq);
  if(it == uac_trans.end())
    return NULL;
  
  return &(it->second);
}

AmSipRequest* AmBasicSipDialog::getUASTrans(unsigned int t_cseq)
{
  TransMap::iterator it = uas_trans.find(t_cseq);
  if(it == uas_trans.end())
    return NULL;
  
  return &(it->second);
}

string AmBasicSipDialog::getUACTransMethod(unsigned int t_cseq)
{
  AmSipRequest* req = getUACTrans(t_cseq);
  if(req != NULL)
    return req->method;

  return string();
}

bool AmBasicSipDialog::getUACTransPending()
{
  return !uac_trans.empty();
}

void AmBasicSipDialog::setStatus(Status new_status) 
{
  DBG("setting SIP dialog status: %s->%s\n",
      getStatusStr(), getStatusStr(new_status));

  status = new_status;
}

const char* AmBasicSipDialog::getStatusStr(AmBasicSipDialog::Status st)
{
  if((st < 0) || (st >= __max_Status))
    return "Invalid";
  else
    return status2str[st];
}

const char* AmBasicSipDialog::getStatusStr()
{
  return getStatusStr(status);
}

string AmBasicSipDialog::getContactHdr()
{
  string contact_uri = SIP_HDR_COLSP(SIP_HDR_CONTACT) "<sip:";

  if(!ext_local_tag.empty()) {
    contact_uri += local_tag + "@";
  }
    
  int oif = getOutboundIf();
  assert(oif >= 0);
  assert(oif < (int)AmConfig::SIP_Ifs.size());
  
  contact_uri += (AmConfig::SIP_Ifs[oif].PublicIP.empty() ?
		  AmConfig::SIP_Ifs[oif].LocalIP :
		  AmConfig::SIP_Ifs[oif].PublicIP);
  contact_uri += ":" + int2str(AmConfig::SIP_Ifs[oif].LocalPort);
  contact_uri += ">" CRLF;

  return contact_uri;
}

string AmBasicSipDialog::getRoute() 
{
  string res;

  if(!outbound_proxy.empty() && (force_outbound_proxy || remote_tag.empty())){
    res += "<" + outbound_proxy + ";lr>";

    if(!route.empty()) {
      res += ",";
    }
  }

  res += route;

  if(!res.empty()) {
    res = SIP_HDR_COLSP(SIP_HDR_ROUTE) + res + CRLF;
  }

  return res;
}

void AmBasicSipDialog::setOutboundInterface(int interface_id) {
  DBG("setting outbound interface to %i\n",  interface_id);
  outbound_interface = interface_id;
}

/** 
 * Computes, set and return the outbound interface
 * based on remote_uri, next_hop_ip, outbound_proxy, route.
 */
int AmBasicSipDialog::getOutboundIf()
{
  if (outbound_interface >= 0)
    return outbound_interface;

  if(AmConfig::SIP_Ifs.size() == 1){
    return (outbound_interface = 0);
  }

  // Destination priority:
  // 1. next_hop
  // 2. outbound_proxy (if 1st req or force_outbound_proxy)
  // 3. first route
  // 4. remote URI
  
  string dest_uri;
  string dest_ip;
  string local_ip;
  multimap<string,unsigned short>::iterator if_it;

  list<host_port> ip_list;
  if(!next_hop.empty() && 
     !parse_next_hop(stl2cstr(next_hop),ip_list) &&
     !ip_list.empty()) {

    dest_ip = c2stlstr(ip_list.front().host);
  }
  else if(!outbound_proxy.empty() &&
	  (remote_tag.empty() || force_outbound_proxy)) {
    dest_uri = outbound_proxy;
  }
  else if(!route.empty()){
    // parse first route
    sip_header fr;
    fr.value = stl2cstr(route);
    sip_uri* route_uri = get_first_route_uri(&fr);
    if(!route_uri){
      ERROR("Could not parse route (local_tag='%s';route='%s')",
	    local_tag.c_str(),route.c_str());
      goto error;
    }

    dest_ip = c2stlstr(route_uri->host);
  }
  else {
    dest_uri = remote_uri;
  }

  if(dest_uri.empty() && dest_ip.empty()) {
    ERROR("No destination found (local_tag='%s')",local_tag.c_str());
    goto error;
  }
  
  if(!dest_uri.empty()){
    sip_uri d_uri;
    if(parse_uri(&d_uri,dest_uri.c_str(),dest_uri.length()) < 0){
      ERROR("Could not parse destination URI (local_tag='%s';dest_uri='%s')",
	    local_tag.c_str(),dest_uri.c_str());
      goto error;
    }

    dest_ip = c2stlstr(d_uri.host);
  }

  if(get_local_addr_for_dest(dest_ip,local_ip) < 0){
    ERROR("No local address for dest '%s' (local_tag='%s')",dest_ip.c_str(),local_tag.c_str());
    goto error;
  }

  if_it = AmConfig::LocalSIPIP2If.find(local_ip);
  if(if_it == AmConfig::LocalSIPIP2If.end()){
    ERROR("Could not find a local interface for resolved local IP (local_tag='%s';local_ip='%s')",
	  local_tag.c_str(), local_ip.c_str());
    goto error;
  }

  setOutboundInterface(if_it->second);
  return if_it->second;

 error:
  WARN("Error while computing outbound interface: default interface will be used instead.");
  setOutboundInterface(0);
  return 0;
}

void AmBasicSipDialog::resetOutboundIf()
{
  setOutboundInterface(-1);
}

/**
 * Update dialog status from UAC Request that we send.
 */
void AmBasicSipDialog::initFromLocalRequest(const AmSipRequest& req)
{
  setRemoteUri(req.r_uri);

  user         = req.user;
  domain       = req.domain;

  setCallid(      req.callid   );
  setLocalTag(    req.from_tag );
  setLocalUri(    req.from_uri );
  setRemoteParty( req.to       );
  setLocalParty(  req.from     );
}

bool AmBasicSipDialog::onRxReqSanity(const AmSipRequest& req)
{
  // Sanity checks
  if(!remote_tag.empty() && !req.from_tag.empty() &&
     (req.from_tag != remote_tag)){
    DBG("remote_tag = '%s'; req.from_tag = '%s'\n",
	remote_tag.c_str(), req.from_tag.c_str());
    reply_error(req, 481, SIP_REPLY_NOT_EXIST);
    return false;
  }

  if (r_cseq_i && req.cseq <= r_cseq){

    if (req.method == SIP_METH_NOTIFY) {
      if (!AmConfig::IgnoreNotifyLowerCSeq) {
	// clever trick to not break subscription dialog usage
	// for implementations which follow 3265 instead of 5057
	string hdrs = SIP_HDR_COLSP(SIP_HDR_RETRY_AFTER)  "0"  CRLF;

	INFO("remote cseq lower than previous ones - refusing request\n");
	// see 12.2.2
	reply_error(req, 500, SIP_REPLY_SERVER_INTERNAL_ERROR, hdrs);
	return false;
      }
    }
    else {
      INFO("remote cseq lower than previous ones - refusing request\n");
      // see 12.2.2
      reply_error(req, 500, SIP_REPLY_SERVER_INTERNAL_ERROR);
      return false;
    }
  }

  r_cseq = req.cseq;
  r_cseq_i = true;

  return true;
}

void AmBasicSipDialog::onRxRequest(const AmSipRequest& req)
{
  DBG("AmBasicSipDialog::onRxRequest(req = %s)\n", req.method.c_str());

  if(logger && (req.method != SIP_METH_ACK)) {
    req.tt.lock_bucket();
    const sip_trans* t = req.tt.get_trans();
    sip_msg* msg = t->msg;
    logger->log(msg->buf,msg->len,&msg->remote_ip,
		&msg->local_ip,msg->u.request->method_str);
    req.tt.unlock_bucket();
  }

  if(!onRxReqSanity(req))
    return;
    
  uas_trans[req.cseq] = req;
    
  // target refresh requests
  if (req.from_uri.length() && 
      (remote_uri.empty() ||
       (req.method == SIP_METH_INVITE || 
	req.method == SIP_METH_UPDATE ||
	req.method == SIP_METH_SUBSCRIBE ||
	req.method == SIP_METH_NOTIFY))) {
    
    // refresh the target
    if (remote_uri != req.from_uri) {
      setRemoteUri(req.from_uri);
      if(nat_handling && req.first_hop) {
	setNextHop(req.remote_ip + ":"
		   + int2str(req.remote_port));
	setNextHop1stReq(false);
      }
    }
  }
  
  // Dlg not yet initialized?
  if(callid.empty()){

    user         = req.user;
    domain       = req.domain;

    setCallid(      req.callid   );
    setRemoteTag(   req.from_tag );
    setLocalUri(    req.r_uri    );
    setRemoteParty( req.from     );
    setLocalParty(  req.to       );
    setRouteSet(    req.route    );
    set1stBranch(   req.via_branch );

    outbound_interface = req.local_if;
  }

  if(onRxReqStatus(req) && hdl)
    hdl->onSipRequest(req);
}

bool AmBasicSipDialog::onRxReplyStatus(const AmSipReply& reply, 
				       TransMap::iterator t_uac_it)
{
  /**
   * Error code list from RFC 5057:
   * those error codes terminate the dialog
   *
   * Note: 408, 480 should only terminate
   *       the usage according to RFC 5057.
   */
  switch(reply.code){
  case 404:
  case 408:
  case 410:
  case 416:
  case 480:
  case 482:
  case 483:
  case 484:
  case 485:
  case 502:
  case 604:
    if(hdl) hdl->onRemoteDisappeared(reply);
    break;
  }
  
  return true;
}

void AmBasicSipDialog::onRxReply(const AmSipReply& reply)
{
  TransMap::iterator t_it = uac_trans.find(reply.cseq);
  if(t_it == uac_trans.end()){
    ERROR("could not find any transaction matching reply: %s\n", 
        ((AmSipReply)reply).print().c_str());
    return;
  }

  DBG("onRxReply(rep = %u %s): transaction found!\n",
      reply.code, reply.reason.c_str());

  updateDialogTarget(reply);
  
  Status saved_status = status;
  if(onRxReplyStatus(reply,t_it) && hdl)
    hdl->onSipReply(t_it->second,reply,saved_status);

  if((reply.code >= 200) && // final reply
     // but not for 2xx INV reply (wait for 200 ACK)
     ((reply.cseq_method != SIP_METH_INVITE) ||
      (reply.code >= 300))) {
       
    uac_trans.erase(t_it);
  }
}

void AmBasicSipDialog::updateDialogTarget(const AmSipReply& reply)
{
  if( (reply.code > 100) && (reply.code < 300) &&
      reply.to_uri.length() &&
      (remote_uri.empty() ||
       (reply.cseq_method.length()==6 &&
	((reply.cseq_method == SIP_METH_INVITE) ||
	 (reply.cseq_method == SIP_METH_UPDATE) ||
	 (reply.cseq_method == SIP_METH_NOTIFY))) ||
       (reply.cseq_method == SIP_METH_SUBSCRIBE)) ) {
    
    setRemoteUri(reply.to_uri);
  }
}

void AmBasicSipDialog::setRemoteTag(const string& new_rt)
{
  if(!new_rt.empty() && remote_tag.empty()){
    remote_tag = new_rt;
  }
}

int AmBasicSipDialog::onTxRequest(AmSipRequest& req, int& flags)
{
  if(hdl) hdl->onSendRequest(req,flags);

  return 0;
}

int AmBasicSipDialog::onTxReply(const AmSipRequest& req, 
				AmSipReply& reply, int& flags)
{
  if(hdl) hdl->onSendReply(req,reply,flags);

  return 0;
}

void AmBasicSipDialog::onReplyTxed(const AmSipRequest& req, 
				   const AmSipReply& reply)
{
  if(hdl) hdl->onReplySent(req, reply);

  if ((reply.code >= 200) && 
      (reply.cseq_method != SIP_METH_CANCEL)) {
    
    uas_trans.erase(reply.cseq);
  }
}

void AmBasicSipDialog::onRequestTxed(const AmSipRequest& req)
{
  if(hdl) hdl->onRequestSent(req);

  if(req.method != SIP_METH_ACK) {
    uac_trans[req.cseq] = req;
    cseq++;
  }
  else {
    uac_trans.erase(req.cseq);
  }
}

int AmBasicSipDialog::reply(const AmSipRequest& req,
			    unsigned int  code,
			    const string& reason,
			    const AmMimeBody* body,
			    const string& hdrs,
			    int flags)
{
  TransMap::const_iterator t_it = uas_trans.find(req.cseq);
  if(t_it == uas_trans.end()){
    ERROR("could not find any transaction matching request cseq\n");
    ERROR("request cseq=%i; reply code=%i; callid=%s; local_tag=%s; "
	  "remote_tag=%s\n",
	  req.cseq,code,callid.c_str(),
	  local_tag.c_str(),remote_tag.c_str());
    return -1;
  }
  DBG("reply: transaction found!\n");
    
  string m_hdrs = hdrs;
  AmSipReply reply;

  reply.code = code;
  reply.reason = reason;
  reply.tt = req.tt;
  if((code > 100) && !(flags & SIP_FLAGS_NOTAG))
    reply.to_tag = ext_local_tag.empty() ? local_tag : ext_local_tag;
  reply.hdrs = m_hdrs;
  reply.cseq = req.cseq;
  reply.cseq_method = req.method;

  if(body != NULL)
    reply.body = *body;

  if(onTxReply(req,reply,flags)){
    DBG("onTxReply failed\n");
    return -1;
  }

  if (!(flags & SIP_FLAGS_VERBATIM)) {
    // add Signature
    if (AmConfig::Signature.length())
      reply.hdrs += SIP_HDR_COLSP(SIP_HDR_SERVER) + AmConfig::Signature + CRLF;
  }

  if ((code < 300) && !(flags & SIP_FLAGS_NOCONTACT)) {
    /* if 300<=code<400, explicit contact setting should be done */
    reply.contact = getContactHdr();
  }

  int ret = SipCtrlInterface::send(reply,local_tag,logger);
  if(ret){
    ERROR("Could not send reply: code=%i; reason='%s'; method=%s;"
	  " call-id=%s; cseq=%i\n",
	  reply.code,reply.reason.c_str(),reply.cseq_method.c_str(),
	  reply.callid.c_str(),reply.cseq);

    return ret;
  }
  else {
    onReplyTxed(req,reply);
  }

  return ret;
}


/* static */
int AmBasicSipDialog::reply_error(const AmSipRequest& req, unsigned int code, 
				  const string& reason, const string& hdrs,
				  msg_logger* logger)
{
  AmSipReply reply;

  reply.code = code;
  reply.reason = reason;
  reply.tt = req.tt;
  reply.hdrs = hdrs;
  reply.to_tag = AmSession::getNewId();

  if (AmConfig::Signature.length())
    reply.hdrs += SIP_HDR_COLSP(SIP_HDR_SERVER) + AmConfig::Signature + CRLF;

  // add transcoder statistics into reply headers
  //addTranscoderStats(reply.hdrs);

  int ret = SipCtrlInterface::send(reply,string(""),logger);
  if(ret){
    ERROR("Could not send reply: code=%i; reason='%s';"
	  " method=%s; call-id=%s; cseq=%i\n",
	  reply.code,reply.reason.c_str(),
	  req.method.c_str(),req.callid.c_str(),req.cseq);
  }

  return ret;
}

int AmBasicSipDialog::sendRequest(const string& method, 
				  const AmMimeBody* body,
				  const string& hdrs,
				  int flags)
{
  AmSipRequest req;

  req.method = method;
  req.r_uri = remote_uri;

  req.from = SIP_HDR_COLSP(SIP_HDR_FROM) + local_party;
  if(!ext_local_tag.empty())
    req.from += ";tag=" + ext_local_tag;
  else if(!local_tag.empty())
    req.from += ";tag=" + local_tag;
    
  req.to = SIP_HDR_COLSP(SIP_HDR_TO) + remote_party;
  if(!remote_tag.empty()) 
    req.to += ";tag=" + remote_tag;
    
  req.cseq = cseq;
  req.callid = callid;
    
  req.hdrs = hdrs;

  req.route = getRoute();

  if(body != NULL) {
    req.body = *body;
  }

  if(onTxRequest(req,flags) < 0)
    return -1;

  if (!(flags & SIP_FLAGS_NOCONTACT)) {
    req.contact = getContactHdr();
  }

  if (!(flags & SIP_FLAGS_VERBATIM)) {
    // add Signature
    if (AmConfig::Signature.length())
      req.hdrs += SIP_HDR_COLSP(SIP_HDR_USER_AGENT) + AmConfig::Signature + CRLF;
    
    req.hdrs += SIP_HDR_COLSP(SIP_HDR_MAX_FORWARDS) + 
      int2str(AmConfig::MaxForwards) + CRLF;
  }

  int res = SipCtrlInterface::send(req, local_tag,
				   remote_tag.empty() || !next_hop_1st_req ?
				   next_hop : "",
				   outbound_interface, logger);
  if(res) {
    ERROR("Could not send request: method=%s; call-id=%s; cseq=%i\n",
	  req.method.c_str(),req.callid.c_str(),req.cseq);
    return res;
  }

  onRequestTxed(req);
  return 0;
}

void AmBasicSipDialog::dump()
{
  DBG("callid = %s\n",callid.c_str());
  DBG("local_tag = %s\n",local_tag.c_str());
  DBG("uac_trans.size() = %u\n",(unsigned int)uac_trans.size());
  if(uac_trans.size()){
    for(TransMap::iterator it = uac_trans.begin();
	it != uac_trans.end(); it++){
	    
      DBG("    cseq = %i; method = %s\n",it->first,it->second.method.c_str());
    }
  }
  DBG("uas_trans.size() = %u\n",(unsigned int)uas_trans.size());
  if(uas_trans.size()){
    for(TransMap::iterator it = uas_trans.begin();
	it != uas_trans.end(); it++){
	    
      DBG("    cseq = %i; method = %s\n",it->first,it->second.method.c_str());
    }
  }
}

void AmBasicSipDialog::setMsgLogger(msg_logger* logger)
{
  if(this->logger) {
    dec_ref(this->logger);
  }

  if(logger){
    inc_ref(logger);
  }

  this->logger = logger;
}
