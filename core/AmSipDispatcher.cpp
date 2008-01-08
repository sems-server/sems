
#include "AmSessionContainer.h"
#include "AmSipDialog.h"
#include "AmUtils.h"
#include "log.h"

#include "AmSipDispatcher.h"

AmSipDispatcher *AmSipDispatcher::_instance;

AmSipDispatcher* AmSipDispatcher::instance()
{
  return _instance ? _instance : ((_instance = new AmSipDispatcher()));
}

void AmSipDispatcher::handleSipMsg(AmSipReply &reply)
{
  if (!AmSessionContainer::instance()->postEvent(reply.local_tag,
      new AmSipReplyEvent(reply))) {
    for (vector<AmSIPEventHandler*>::iterator it = 
        reply_handlers.begin(); it != reply_handlers.end(); it++) 
      if ((*it)->onSipReply(reply))
        break;
  }
}

void AmSipDispatcher::handleSipMsg(AmSipRequest &req)
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
		
    DBG("method: `%s' [%zd].\n", req.method.c_str(), req.method.length());
    
    if((req.method == "INVITE")||
       ((req.method == "REFER") && !local_tag.empty())){
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
			
      if (AmSessionContainer::instance()->startSessionUAC(n_req) != NULL)
        AmSipDialog::reply_error(req,202,"Accepted");
      else 
        AmSipDialog::reply_error(req,500,"Not supported here");
    } else {
      if(!local_tag.empty() || req.method == "CANCEL"){
        AmSipDialog::reply_error(req,481,
          "Call leg/Transaction does not exist");
      } else {
        //TODO: reply some 4xx.
        ERROR("sorry, we don't support beginning a new session with "
          "a '%s' message\n", req.method.c_str());
        AmSipDialog::reply_error(req,500,"Not supported here");
        return;
      }
    }
  }
}
