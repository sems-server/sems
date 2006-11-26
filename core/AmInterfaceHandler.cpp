#include "AmInterfaceHandler.h"
#include "AmCtrlInterface.h"
#include "AmSessionContainer.h"
#include "AmServer.h" // FIFO_VERSION
#include "AmUtils.h"
#include "AmSipRequest.h"
#include "AmConfig.h"
#include "AmUtils.h"

#include "log.h"

#include <assert.h>

#define SAFECTRLCALL0(fct)\
                           {\
                               if(ctrl->fct() == -1){\
                                   ERROR("%s returned -1\n",#fct);\
                                   return -1;\
                               }\
                           }

#define SAFECTRLCALL1(fct,arg1)\
                           {\
                               if(ctrl->fct(arg1) == -1){\
                                   ERROR("%s returned -1\n",#fct);\
                                   return -1;\
                               }\
                           }


AmInterfaceHandler::~AmInterfaceHandler()
{}

//
// AmRequestHandler methods
//

static AmRequestHandler _request_handler;

AmRequestHandler* AmRequestHandler::get()
{
    return &_request_handler;
}

int AmRequestHandler::handleRequest(AmCtrlInterface* ctrl)
{
    string            version;
    string            fct_name;
    string            cmd;
    string::size_type pos;

    SAFECTRLCALL1(getParam,version);
    if (version == "") {
	// some odd trailer from previous request -- ignore
	ERROR("odd trailer\n");
	return -1;
    }
    if(version != FIFO_VERSION){
	ERROR("wrong FIFO Interface version: %s\n",version.c_str());
	return -1;
    }

    SAFECTRLCALL1(getParam,fct_name);
    if((pos = fct_name.find('.')) != string::npos){

	cmd = fct_name.substr(pos+1,string::npos);
	fct_name = fct_name.substr(0,pos);
    }

    if(fct_name == "sip_request")
	return execute(ctrl,cmd);
    
    AmRequestHandlerFct* fct = getFct(fct_name);
    if(!fct){
 	ERROR("unknown request function: '%s'\n",fct_name.c_str());
	return -1;
    }
    
    return fct->execute(ctrl,cmd);
}

int AmRequestHandler::execute(AmCtrlInterface* ctrl, const string& cmd)
{
    AmSipRequest req;

    if(cmd.empty()){
	ERROR("AmRequestHandler::execute: parameter plug-in name missing.\n");
	return -1;
    }

    //req.cmd = cmd;

#define READ_PARAMETER(p)\
             {\
		 SAFECTRLCALL1(getParam,p);\
		 DBG("%s = <%s>\n",#p,p.c_str());\
	     }

    READ_PARAMETER(req.method);
    READ_PARAMETER(req.user);
    READ_PARAMETER(req.domain);
    READ_PARAMETER(req.dstip);    // will be taken for UDP's local peer IP & Contact
    READ_PARAMETER(req.port);     // will be taken for Contact
    READ_PARAMETER(req.r_uri);    // ??? will be taken for the answer's Contact header field ???
    READ_PARAMETER(req.from_uri); // will be taken for subsequent request uri
    READ_PARAMETER(req.from);
    READ_PARAMETER(req.to);
    READ_PARAMETER(req.callid);
    READ_PARAMETER(req.from_tag);
    READ_PARAMETER(req.to_tag);

    string cseq_str;
    READ_PARAMETER(cseq_str);
    
    if(sscanf(cseq_str.c_str(),"%u", &req.cseq) != 1){
	ERROR("invalid cseq number '%s'\n",cseq_str.c_str());
	return -1;
    }
    DBG("cseq = <%s>(%i)\n",cseq_str.c_str(),req.cseq);
    
    READ_PARAMETER(req.key);
    READ_PARAMETER(req.route);
    READ_PARAMETER(req.next_hop);

#undef READ_PARAMETER

    SAFECTRLCALL1(getLines,req.hdrs);
    DBG("hdrs = <%s>\n",req.hdrs.c_str());

    req.cmd = cmd;

    SAFECTRLCALL1(getLines,req.body);
    DBG("body = <%s>\n",req.body.c_str());
    
    if(req.from.empty() || 
       req.to.empty() || 
       req.callid.empty() || 
       req.from_tag.empty()) {

	throw string("AmRequestHandler::execute: mandatory parameter "
		     "is empty (from|to|callid|from_tag)\n");
    }

    DBG("Request OK: dispatch it!\n");
    dispatch(req);
    
    return 0;
    
}


//
// AmReplyHandler methods
//

AmReplyHandler* AmReplyHandler::_instance=0;

AmReplyHandler::AmReplyHandler(AmCtrlInterface* ctrl)
    : m_ctrl(ctrl)
{
}

AmReplyHandler* AmReplyHandler::get()
{
    if(!_instance){
	AmCtrlInterface* ctrl = AmCtrlInterface::getNewCtrl();
	if(ctrl->init(AmConfig::ReplySocketName)){
	    ERROR("could not initialize the reply socket '%s'\n",
		  AmConfig::ReplySocketName.c_str());
	    delete ctrl;
	    return NULL;
	}
	
	_instance = new AmReplyHandler(ctrl);
    }

    return _instance;
}

int AmReplyHandler::handleRequest(AmCtrlInterface* ctrl)
{
    AmSipReply reply;

    string tmp_str;
    SAFECTRLCALL1(getParam,tmp_str);
    
    DBG("response from Ser: %s\n",tmp_str.c_str());
    if( parse_return_code(tmp_str.c_str(),// res_code_str,
			  reply.code,reply.reason) == -1 ){
	ERROR("while parsing return code from Ser.\n");
	//cleanup(ctrl);
	return -1;
    }

    /* Parse complete response:
     *
     *   [next_request_uri->cmd.from_uri]CRLF
     *   [next_hop->cmd.next_hop]CRLF
     *   [route->cmd.route]CRLF
     *   ([headers->n_cmd.hdrs]CRLF)*
     *   CRLF
     *   ([body->body]CRLF)*
     */
	
    SAFECTRLCALL1(getParam,reply.next_request_uri);
    SAFECTRLCALL1(getParam,reply.next_hop);
    SAFECTRLCALL1(getParam,reply.route);

    SAFECTRLCALL1(getLines,reply.hdrs);
    SAFECTRLCALL1(getLines,reply.body);

    if(reply.hdrs.empty()){
	ERROR("reply is missing headers: <%i %s>\n",
	      reply.code,reply.reason.c_str());
	return -1;
    }
    
    reply.local_tag = getHeader(reply.hdrs,"From");
    reply.local_tag = extract_tag(reply.local_tag);

    reply.remote_tag = getHeader(reply.hdrs,"To");
    reply.remote_tag = extract_tag(reply.remote_tag);

    string cseq_str;
    cseq_str   = getHeader(reply.hdrs,"CSeq");
    if(str2i(cseq_str,reply.cseq)){
	ERROR("could not parse CSeq header\n");
	return -1;
    }

    AmSessionContainer::instance()->postEvent(
	    reply.local_tag,
	    new AmSipReplyEvent(reply));

//     if(reply.code >= 200)
// 	cleanup(ctrl);

    return 0;
}

AmRequestHandlerFct* AmRequestHandler::getFct(const string& name)
{
    AmRequestHandlerFct* fct=NULL;

    fct_map_mut.lock();

    if(fct_map.find(name) != fct_map.end())
	fct = fct_map[name];

    fct_map_mut.unlock();

    return fct;
}

void AmRequestHandler::registerFct(const string& name, AmRequestHandlerFct* fct)
{
    fct_map_mut.lock();
    fct_map.insert(make_pair(name,fct));
    fct_map_mut.unlock();
}

void AmRequestHandler::dispatch(AmSipRequest& req)
{
    string callid     = req.callid;
    string remote_tag = req.from_tag;
    string local_tag  = req.to_tag;

    bool sess_exists;
    AmSipRequestEvent* ev = new AmSipRequestEvent(req);
    AmSessionContainer* sess_cont = AmSessionContainer::instance();

    if(local_tag.empty())
	sess_exists = sess_cont->postEvent(callid,remote_tag,ev);
    else
	sess_exists = sess_cont->postEvent(local_tag,ev);

    if(!sess_exists){

	if(req.method == "INVITE"){

		sess_cont->startSessionUAS(req);

	} else if(req.method == "REFER"){
		// Out-of-dialog REFER
		AmSipRequest n_req;
		n_req.method   = "INVITE";
		n_req.dstip    = AmConfig::LocalIP;

		n_req.from     = req.to;
		n_req.from_uri = req.r_uri;
		n_req.from_tag = AmSession::getNewId();
		n_req.user     = req.user;
		
		n_req.r_uri    = uri_from_name_addr(getHeader(req.hdrs, "Refer-To"));
		n_req.to       = getHeader(req.hdrs, "Refer-To");
		n_req.to_tag   = "";
  		n_req.cmd      = req.cmd; // application from REFER
		
		n_req.callid   = AmSession::getNewId() + "@" + AmConfig::LocalIP;
    
		if (AmSessionContainer::instance()->startSessionUAC(n_req) 
			!= NULL)
			AmSipDialog::reply_error(req,202,"Accepted");
		else 
			AmSipDialog::reply_error(req,500,"Not supported here");

	}	else {
	    
	    if(!local_tag.empty() || req.method == "CANCEL"){
		AmSipDialog::reply_error(req,481,"Call leg/Transaction does not exist");
	    }
	    else {
		//TODO: reply some 4xx.
		ERROR("sorry, we don't support beginning a new session with a '%s' message\n",
		      req.method.c_str());
		AmSipDialog::reply_error(req,500,"Not supported here");
		return;
	    }
	}
    }
}
