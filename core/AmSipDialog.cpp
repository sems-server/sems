#include "AmSipDialog.h"
#include "AmConfig.h"
#include "AmSession.h"
#include "AmUtils.h"
#include "AmCtrlInterface.h"
#include "AmServer.h"
#include "AmInterfaceHandler.h"

AmSipDialog::~AmSipDialog()
{
    DBG("callid = %s\n",callid.c_str());
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

void AmSipDialog::updateStatus(const AmSipRequest& req)
{
    if(uas_trans.find(req.cseq) == uas_trans.end())
	uas_trans[req.cseq] = AmSipTransaction(req.method,req.cseq);

    remote_uri = req.from_uri;
    if(callid.empty()){
	callid       = req.callid;
	remote_tag   = req.from_tag;
	user         = req.user;
	domain       = req.domain;
	local_uri    = req.r_uri;
	remote_party = req.from;
	local_party  = req.to;

	sip_ip       = req.dstip;
	sip_port     = req.port;

	setRoute(req.route);
	next_hop   = req.next_hop;
    }
}

int AmSipDialog::updateStatusReply(const AmSipRequest& req, unsigned int code)
{
    TransMap::iterator t_it = uas_trans.find(req.cseq);
    if(t_it == uas_trans.end()){
	ERROR("could not find any transaction matching request\n");
	ERROR("method=%s; callid=%s; local_tag=%s; remote_tag=%s; cseq=%i\n",
	      req.method.c_str(),callid.c_str(),local_tag.c_str(),
	      remote_tag.c_str(),req.cseq);
	return -1;
    }
    DBG("reply: transaction found!\n");
    
    AmSipTransaction& t = t_it->second;

    //t->reply_code = code;
    switch(status){

    case Disconnected:
    case Pending:
	if(t.method == "INVITE"){
	
	    if(req.method == "CANCEL"){
		
		// wait for somebody
		// to answer 487
		return 0;
	    }

	    if(code < 200)
		status = Pending;
	    else if(code < 300)
		status = Connected;
	    else
		status = Disconnected;
	}
	
	break;
    case Connected:
    case Disconnecting:
	if(t.method == "BYE"){
	    
	    if((code < 300) && (code >= 200))
		status = Disconnected;
	}
	break;
    }

    if(code >= 200){
	DBG("req.method = %s; t.method = %s\n",
	    req.method.c_str(),t.method.c_str());

	uas_trans.erase(t_it);
    }

    return 0;
}

void AmSipDialog::updateStatus(const AmSipReply& reply)
{
    TransMap::iterator t_it = uac_trans.find(reply.cseq);
    if(t_it == uac_trans.end()){
	ERROR("could not find any transaction matching reply\n");
	return;
    }
    DBG("updateStatus(reply): transaction found!\n");

    AmSipTransaction& t = t_it->second;

    //t->reply_code = reply.code;

    if(remote_tag.empty() && !reply.remote_tag.empty())
	remote_tag = reply.remote_tag;

    // allow route overwritting
    if(status < Connected) {

	if(!reply.route.empty())
	    setRoute(reply.route);

	next_hop = reply.next_hop;
    }

    remote_uri = reply.next_request_uri;

    switch(status){
    case Disconnecting:
	if( t.method == "INVITE" ){

	    if(reply.code == 487){
		// CANCEL accepted
		status = Disconnected;
	    }
	    else {
		// CANCEL rejected
		sendRequest("BYE");
	    }
	}
	break;

    case Pending:
    case Disconnected:
	if(t.method != "BYE"){

	    if(reply.code < 200)
		status = Pending;
	    else if(reply.code >= 300)
		status = Disconnected;
	    else
		status = Connected;
	}
	break;
    default:
	break;
    }

    if(reply.code >= 200)
	uac_trans.erase(t_it);
}

string AmSipDialog::getContactHdr()
{
    if(!contact_uri.empty())
	 return contact_uri;

    contact_uri = "Contact: <sip:";
    
    if(user.empty() || !AmConfig::PrefixSep.empty())
	contact_uri += CONTACT_USER_PREFIX;

    if(!AmConfig::PrefixSep.empty())
	contact_uri += AmConfig::PrefixSep;

    if(!user.empty())
	contact_uri += user;

    contact_uri += "@";
    
    if(sip_ip.empty())
	contact_uri += "!!"; // Ser will replace that...
    else {
#ifdef SUPPORT_IPV6
	if(sip_ip.find('.') != string::npos)
	    contact_uri += sip_ip;
	else
	    contact_uri += "[" + sip_ip + "]";
#else
	contact_uri += sip_ip;
#endif
    }

    if(!sip_port.empty())
	contact_uri += ":" + sip_port;

    contact_uri += ">\n";
    
    return contact_uri;
}

int AmSipDialog::reply(const AmSipRequest& req,
		       unsigned int  code,
		       const string& reason,
		       const string& content_type,
		       const string& body,
		       const string& hdrs)
{
    string m_hdrs = hdrs;

    if(hdl)
	hdl->onSendReply(req,code,reason,
			 content_type,body,m_hdrs);

    string reply_sock = "/tmp/" + AmSession::getNewId();
    string code_str = int2str(code);

    string msg = 
	":t_reply:" + reply_sock + "\n" +
	code_str + "\n" + 
	reason + "\n" + 
	req.key + "\n" + 
 	local_tag + "\n";

    if(!m_hdrs.empty())
	msg += m_hdrs;

    msg += getContactHdr();

    if(!body.empty())
	msg += "Content-Type: " + content_type + "\n";

    msg += ".\n";

    if(!body.empty())
	msg += body;

    msg += ".\n\n";

    if(updateStatusReply(req,code))
	return -1;

    return send_reply(msg,reply_sock);
}

/* static */
int AmSipDialog::reply_error(const AmSipRequest& req, unsigned int code, 
			     const string& reason, const string& hdrs)
{
    string reply_sock = "/tmp/" + AmSession::getNewId();
    string code_str = int2str(code);

    string msg = 
      ":t_reply:" + reply_sock + "\n" +
      code_str + "\n" + 
      reason + "\n" + 
      req.key + "\n" + 
      AmSession::getNewId() + "\n";
      if(!hdrs.empty())
	msg += hdrs;
      else 
	msg +=  "\n";

      msg += ".\n.\n\n";
    
    return send_reply(msg,reply_sock);
}

/* static */
int AmSipDialog::send_reply(const string& msg, const string& reply_sock)
{
    auto_ptr<AmCtrlInterface> ctrl;
    ctrl.reset(AmCtrlInterface::getNewCtrl(NULL));

    if(ctrl->init(reply_sock) || 
       ctrl->sendto(AmConfig::SerSocketName,msg.c_str(),msg.length())){
	
	ERROR("while sending reply to Ser\n");
	return -1;
    }

    if(ctrl->wait4data(500) < 1){ // 100 ms
	ERROR("while waiting for Ser's response\n");
	return -1;
    }

    string status_line;
    if(ctrl->cacheMsg() || 
       ctrl->getParam(status_line)) 
	return -1;

    unsigned int res_code;
    string res_reason;
    if(parse_return_code(status_line.c_str(),res_code,res_reason))
	return -1;
    
    if( (res_code < 200) ||
	(res_code >= 300) ) {
	
	ERROR("AmSipDialog::reply: ser answered: %i %s\n",res_code,res_reason.c_str());
	return -1;
    }

    return 0;
}


int AmSipDialog::bye()
{
    switch(status){
    case Disconnecting:
    case Connected:
	status = Disconnected;
	return sendRequest("BYE");
    case Pending:
	status = Disconnecting;
	if(getUACTransPending())
	    return cancel();
	else {
	    // missing AmSipRequest to be able
	    // to send the reply on behalf of the app.
	    ERROR("bye(): Dialog should have"
		  " been terminated by the app !!!\n");
	}
    default:
	if(getUACTransPending())
	    return cancel();
	else {
	    DBG("bye(): we are not connected "
		"(status=%i). do nothing!\n",status);
	}
	return 0;
    }	
}

int AmSipDialog::reinvite(const string& hdrs,  
			  const string& content_type,
			  const string& body)
{
    switch(status){
    case Connected:
	return sendRequest("INVITE", content_type, body, hdrs);
    case Disconnecting:
    case Pending:
      DBG("reinvite(): we are not yet connected."
	    "(status=%i). do nothing!\n",status);

      return 0;
    default:
      DBG("reinvite(): we are not connected "
	  "(status=%i). do nothing!\n",status);
      return 0;
    }	
}

int AmSipDialog::update(const string& hdrs)
{
    switch(status){
    case Connected:
	return sendRequest("UPDATE", "", "", hdrs);
    case Disconnecting:
    case Pending:
      DBG("update(): we are not yet connected."
	    "(status=%i). do nothing!\n",status);

      return 0;
    default:
      DBG("update(): we are not connected "
	  "(status=%i). do nothing!\n",status);
      return 0;
    }	
}

int AmSipDialog::cancel()
{
    int cancel_cseq = -1;
    TransMap::reverse_iterator t;

    for(t = uac_trans.rbegin();
	t != uac_trans.rend(); t++) {

	if(t->second.method == "INVITE"){
	    cancel_cseq = t->second.cseq;
	    break;
	}
    }
    
    if(t == uac_trans.rend()){
	ERROR("could not find INVITE transaction to cancel\n");
	return -1;
    }
    
    string reply_sock = "/tmp/" + AmSession::getNewId();
    string msg = ":t_uac_cancel:" + reply_sock + "\n"
	+ callid + "\n"
 	+ int2str(cancel_cseq) + "\n\n";

    auto_ptr<AmCtrlInterface> ctrl;
    ctrl.reset(AmCtrlInterface::getNewCtrl(NULL));

    if(ctrl->init(reply_sock) || 
       ctrl->sendto(AmConfig::SerSocketName,msg.c_str(),msg.length())){
	
	ERROR("while sending reply to Ser\n");
	return -1;
    }

    if(ctrl->wait4data(50000) < 1){ // 50 ms
	ERROR("while waiting for Ser's response: %s\n",strerror(errno));
	return -1;
    }

    string status_line;
    if(ctrl->cacheMsg() || 
       ctrl->getParam(status_line)) 
	return -1;

    unsigned int res_code;
    string res_reason;
    if(parse_return_code(status_line.c_str(),res_code,res_reason))
	return -1;
    
    if( (res_code < 200) ||
	(res_code >= 300) ) {
	
	ERROR("AmSipDialog::cancel: ser answered: %i %s\n",res_code,res_reason.c_str());
	return -1;
    }

    return 0;
}

int AmSipDialog::sendRequest(const string& method, 
			     const string& content_type,
			     const string& body,
			     const string& hdrs)
{
    string msg,ser_cmd;
    string m_hdrs = hdrs;

    if(hdl)
	hdl->onSendRequest(method,content_type,body,m_hdrs);

    msg = ":t_uac_dlg:" + AmConfig::ReplySocketName + "\n"
	+ method + "\n"
	+ remote_uri + "\n";
    
    if(next_hop.empty())
	msg += ".";
    else
	msg += next_hop;
    
    msg += "\n";
    
    msg += "From: " + local_party;
    if(!local_tag.empty())
	msg += ";tag=" + local_tag;
    
    msg += "\n";
    
    msg += "To: " + remote_party;
    if(!remote_tag.empty()) 
	msg += ";tag=" + remote_tag;
    
    msg += "\n";
    
    msg += "CSeq: " + int2str(cseq) + " " + method + "\n"
	+ "Call-ID: " + callid + "\n";
    
    msg += getContactHdr();
    
    if(!hdrs.empty()){
	msg += hdrs;
	if(hdrs[hdrs.length()-1] != '\n')
	    msg += "\n";
    }

    if(!route.empty())
	msg += getRoute();
    
    if(!body.empty())
	msg += "Content-Type: " + content_type + "\n";
    
    msg += ".\n" // EoH
	+ body + ".\n\n";

    AmReplyHandler* rh = AmReplyHandler::get(); // singleton
    AmCtrlInterface* ctrl = rh->getCtrl();


    if(ctrl->sendto(AmConfig::SerSocketName,msg.c_str(),msg.length())){
	
	ERROR("while sending request to Ser\n");
	return -1;
    }

    uac_trans[cseq] = AmSipTransaction(method,cseq);

    // increment for next request
    cseq++;
    
    return 0;
}

bool AmSipDialog::match_cancel(const AmSipRequest& cancel_req)
{
    TransMap::iterator t = uas_trans.find(cancel_req.cseq);

    if((t != uas_trans.end()) && (t->second.method == "INVITE"))
	return true;

    return false;
}

string AmSipDialog::getRoute()
{
    string r_set("");
    for(vector<string>::iterator it = route.begin();
	it != route.end(); it++) {

	r_set += "Route: " + *it + "\n";
    }

    return r_set;
}

void AmSipDialog::setRoute(const string& n_route)
{
    string m_route = n_route;
    if(!m_route.empty() && (m_route.find("Route: ")!=string::npos))
	m_route = m_route.substr(7/*sizeof("Route: ")*/);
    
    route.clear();
    while(!m_route.empty()){
	
	string::size_type comma_pos;
	
	comma_pos = m_route.find(',');
	//route += "Route: " + m_route.substr(0,comma_pos) + "\n";
	route.push_back(m_route.substr(0,comma_pos));
	
	if(comma_pos != string::npos)
	    m_route = m_route.substr(comma_pos+1);
	else
	    m_route = "";
    }
}
