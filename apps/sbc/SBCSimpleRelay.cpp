#include "SBCSimpleRelay.h"

#include "AmSession.h"
#include "AmB2BSession.h"
#include "AmEventDispatcher.h"
#include "AmEventQueueProcessor.h"
#include "SBC.h"
#include "RegisterDialog.h"
#include "ReplacesMapper.h"

/**
 * SimpleRelayDialog
 */

void SimpleRelayDialog::initCCModules(SBCCallProfile &profile,
				      vector<AmDynInvoke*> &cc_modules)
{
  // init extended call control modules
  for (vector<AmDynInvoke*>::iterator cc_mod = cc_modules.begin();
       cc_mod != cc_modules.end(); ++cc_mod)
  {
    // get extended CC interface
    try {
      AmArg args, ret;
      (*cc_mod)->invoke("getExtendedInterfaceHandler", args, ret);
      ExtendedCCInterface *iface =
	dynamic_cast<ExtendedCCInterface*>(ret[0].asObject());
      if (iface) {
        CCModuleInfo mod_info;
        iface->init(profile, this, mod_info.user_data);
        mod_info.module = iface;
        cc_ext.push_back(mod_info);
      }
    }
    catch (...) { /* ignore errors */ }
  }
}

SimpleRelayDialog::SimpleRelayDialog(SBCCallProfile &profile, 
				     vector<AmDynInvoke*> &cc_modules, 
				     atomic_ref_cnt* parent_obj)
  : AmBasicSipDialog(this),
    AmEventQueue(this),
    parent_obj(parent_obj),
    transparent_dlg_id(false),
    keep_vias(false),
    fix_replaces_ref(false),
    finished(false)
{
  if(parent_obj) {
    inc_ref(parent_obj);
  }
  initCCModules(profile, cc_modules);
}

SimpleRelayDialog::SimpleRelayDialog(atomic_ref_cnt* parent_obj)
  : AmBasicSipDialog(this),
    AmEventQueue(this),
    parent_obj(parent_obj),
    transparent_dlg_id(false),
    keep_vias(false),
    fix_replaces_ref(false),
    finished(false)
{
  if(parent_obj) {
    inc_ref(parent_obj);
  }
}

SimpleRelayDialog::~SimpleRelayDialog()
{
  DBG("~SimpleRelayDialog: local_tag = %s\n",local_tag.c_str());
  if(!local_tag.empty()) {
    AmEventDispatcher::instance()->delEventQueue(local_tag);
  }
}

int SimpleRelayDialog::relayRequest(const AmSipRequest& req)
{
  relayed_reqs[cseq] = req.cseq;

  string hdrs = req.hdrs;
  if(headerfilter.size()) inplaceHeaderFilter(hdrs, headerfilter);

  if (fix_replaces_ref && req.method == SIP_METH_REFER) {
    fixReplaces(hdrs, false);
  }

  if(!append_headers.empty()) hdrs += append_headers;

  if(keep_vias)
    hdrs = req.vias + hdrs;

  if(sendRequest(req.method,&req.body,hdrs,SIP_FLAGS_VERBATIM, req.max_forwards - 1)) {

    AmSipReply error;
    error.code = 500;
    error.reason = SIP_REPLY_SERVER_INTERNAL_ERROR;
    error.cseq = req.cseq;

    B2BSipReplyEvent* b2b_ev = new B2BSipReplyEvent(error,true,req.method, getLocalTag());
    if(!AmEventDispatcher::instance()->post(other_dlg,b2b_ev)) {
      delete b2b_ev;
    }
    return -1;
  }
  
  return 0;
}

int SimpleRelayDialog::relayReply(const AmSipReply& reply)
{
  AmSipRequest* uas_req = getUASTrans(reply.cseq);
  if(!uas_req){
    //TODO: request already replied???
    ERROR("request already replied???\n");
    return -1;
  }

  string hdrs = reply.hdrs;
  if(headerfilter.size()) inplaceHeaderFilter(hdrs, headerfilter);
  //if(!append_headers.empty()) hdrs += append_headers;

  // reply translations
  int code = reply.code;
  string reason = reply.reason;

  map<unsigned int, pair<unsigned int, string> >::iterator it =
    reply_translations.find(code);

  if (it != reply_translations.end()) {
    DBG("translating reply %u %s => %u %s\n",
	code, reason.c_str(), it->second.first, it->second.second.c_str());

    code = it->second.first;
    reason = it->second.second;
  }

  if(transparent_dlg_id &&
     ext_local_tag.empty() &&
     !reply.to_tag.empty()) {

    setExtLocalTag(reply.to_tag);
  }

  if(this->reply(*uas_req,code,reason,&reply.body,
		 hdrs,SIP_FLAGS_VERBATIM)) {
    //TODO: what can we do???
    return -1;
  }

  return 0;
}

void SimpleRelayDialog::process(AmEvent* ev)
{
  AmSipEvent* sip_ev = dynamic_cast<AmSipEvent*>(ev);
  if(sip_ev) {
    (*sip_ev)(this);
    return;
  }

  B2BSipEvent* b2b_ev = dynamic_cast<B2BSipEvent*>(ev);
  if(b2b_ev) {
    if(b2b_ev->event_id == B2BSipRequest) {
      const AmSipRequest& req = ((B2BSipRequestEvent*)b2b_ev)->req;
      onB2BRequest(req);
      return;
    }
    else if(b2b_ev->event_id == B2BSipReply){
      const AmSipReply& reply = ((B2BSipReplyEvent*)b2b_ev)->reply;
      onB2BReply(reply);
      //TODO: if error, kill dialog???
      return;
    }
  }

  B2BEvent* b2b = dynamic_cast<B2BEvent*>(ev);
  if(b2b && b2b->event_id == B2BTerminateLeg){
    DBG("received terminate event from other leg");
    terminate();
    return;
  }

  ERROR("received unknown event\n");
}

bool SimpleRelayDialog::processingCycle()
{
  DBG("vv [%s|%s] %i usages (%s) vv\n",
      callid.c_str(),local_tag.c_str(),
      getUsages(), terminated() ? "term" : "active");

  try {
    processEvents();
  }
  catch (...) {
    ERROR("unhandled exception when processing events, terminating dialog");
    if (!other_dlg.empty()) {
      B2BEvent *e = new B2BEvent(B2BTerminateLeg);
      if (!AmEventDispatcher::instance()->post(other_dlg, e)) delete e;
    }
    terminate();
  }

  DBG("^^ [%s|%s] %i usages (%s) ^^\n",
      callid.c_str(),local_tag.c_str(),
      getUsages(), terminated() ? "term" : "active");

  return !terminated();
}

void SimpleRelayDialog::finalize()
{
  AmBasicSipDialog::finalize();

  for (list<CCModuleInfo>::iterator i = cc_ext.begin();
       i != cc_ext.end(); ++i) {
    i->module->finalize(i->user_data);
  }

  DBG("finalize(): tag=%s\n",local_tag.c_str());
  AmEventQueue::finalize();
  if(parent_obj) {
    // this might delete us!!!
    atomic_ref_cnt* p_obj = parent_obj;
    parent_obj = NULL;
    dec_ref(p_obj);
  }
}

void SimpleRelayDialog::onB2BRequest(const AmSipRequest& req)
{
  for (list<CCModuleInfo>::iterator i = cc_ext.begin();
       i != cc_ext.end(); ++i) {
    i->module->onB2BRequest(req, i->user_data);
  }
  relayRequest(req);
}

void SimpleRelayDialog::onB2BReply(const AmSipReply& reply)
{
  for (list<CCModuleInfo>::iterator i = cc_ext.begin();
       i != cc_ext.end(); ++i) {
    i->module->onB2BReply(reply, i->user_data);
  }
  if(reply.code >= 200)
    finished = true;

  relayReply(reply);
}

void SimpleRelayDialog::onSendRequest(AmSipRequest& req, int& flags)
{
  if(auth_h.get())
    auth_h->onSendRequest(req,flags);
}

void SimpleRelayDialog::onSipRequest(const AmSipRequest& req)
{
  for (list<CCModuleInfo>::iterator i = cc_ext.begin();
       i != cc_ext.end(); ++i) {
    i->module->onSipRequest(req, i->user_data);
  }
  if(other_dlg.empty()) {
    reply(req,481,SIP_REPLY_NOT_EXIST);
    return;
  }

  B2BSipRequestEvent* b2b_ev = new B2BSipRequestEvent(req,true);
  if(!AmEventDispatcher::instance()->post(other_dlg,b2b_ev)) {
    DBG("other dialog has already been deleted: reply 481");
    reply(req,481,SIP_REPLY_NOT_EXIST);
    delete b2b_ev;
  }
}

void SimpleRelayDialog::onSipReply(const AmSipRequest& req,
				   const AmSipReply& reply, 
				   AmBasicSipDialog::Status old_dlg_status)
{
  unsigned int cseq_before = cseq;
  if(auth_h.get() && auth_h->onSipReply(req,reply,old_dlg_status)) {

    if (cseq_before != cseq) {
      DBG("uac_auth consumed reply with cseq %d and resent with cseq %d; "
          "updating relayed_req map\n", reply.cseq, cseq_before);

      RelayMap::iterator t = relayed_reqs.find(reply.cseq);
      if (t != relayed_reqs.end()) {
        relayed_reqs[cseq_before] = t->second;
        relayed_reqs.erase(t);
        DBG("updated relayed_req (UAC trans): CSeq %u -> %u\n",
            reply.cseq, cseq_before);
      }

      return;
    }
  }

  for (list<CCModuleInfo>::iterator i = cc_ext.begin();
       i != cc_ext.end(); ++i) {
    i->module->onSipReply(req, reply, old_dlg_status, i->user_data);
  }
  if(reply.code >= 200)
    finished = true;

  if(other_dlg.empty()) {
    DBG("other dialog has already been deleted: what can we do now???");
    return;
  }

  RelayMap::iterator t_req_it = relayed_reqs.find(reply.cseq);
  if(t_req_it == relayed_reqs.end()) {
    DBG("reply to a local request ????\n");
    return;
  }
  
  B2BSipReplyEvent* b2b_ev = new B2BSipReplyEvent(reply,true,req.method,getLocalTag());
  b2b_ev->reply.cseq = t_req_it->second;
  if(reply.code >= 200)
    relayed_reqs.erase(t_req_it);

  if(!AmEventDispatcher::instance()->post(other_dlg,b2b_ev)) {
    DBG("other dialog has already been deleted: what can we do now???");
    delete b2b_ev;
  }
}

void SimpleRelayDialog::onRequestSent(const AmSipRequest& req)
{
}

void SimpleRelayDialog::onReplySent(const AmSipRequest& req,
				     const AmSipReply& reply)
{
}

void SimpleRelayDialog::onRemoteDisappeared(const AmSipReply& reply)
{
  DBG("### reply.code = %i ###\n",reply.code);
  terminate();
}

void SimpleRelayDialog::onLocalTerminate(const AmSipReply& reply)
{
  DBG("### reply.code = %i ###\n",reply.code);
  terminate();
}

int SimpleRelayDialog::initUAC(const AmSipRequest& req, 
				const SBCCallProfile& cp)
{
  for (list<CCModuleInfo>::iterator i = cc_ext.begin(); i != cc_ext.end(); ++i) {
    i->module->initUAC(req, i->user_data);
  }
  setLocalTag(AmSession::getNewId());

  AmEventDispatcher* ev_disp = AmEventDispatcher::instance();
  if(!ev_disp->addEventQueue(local_tag,this)) {
    ERROR("addEventQueue(%s,%p) failed.\n",
	  local_tag.c_str(),this);
    reply_error(req,500,SIP_REPLY_SERVER_INTERNAL_ERROR);
    return -1;
  }

  AmSipRequest n_req(req);
  n_req.from_tag = local_tag;

  if(req.method != SIP_METH_REGISTER) {
    AmUriParser ruri;
    ruri.uri = req.r_uri;
    if(!ruri.parse_uri()) {
      DBG("Error parsing R-URI '%s'\n",ruri.uri.c_str());
      reply_error(req,400,"Failed to parse R-URI");
    }

    if(cp.contact_hiding) {
      if(RegisterDialog::decodeUsername(req.user,ruri)) {
	n_req.r_uri = ruri.uri_str();
      }
    }
    else if(cp.reg_caching) {
      try {
	n_req.r_uri = cp.retarget(req.user,*this);
      }
      catch(AmSession::Exception& e) {
	reply_error(req,e.code,e.reason);
	return -1;
      }
    }
  }

  ParamReplacerCtx ctx(&cp);
  if((cp.apply_b_routing(ctx,n_req,*this) < 0) ||
     (cp.apply_common_fields(ctx,n_req) < 0) ) {
    reply_error(req,500,SIP_REPLY_SERVER_INTERNAL_ERROR);
    return -1;
  }

  if(cp.transparent_dlg_id){
    setExtLocalTag(req.from_tag);
    cseq = req.cseq;
  }
  else if(n_req.callid == req.callid)
    n_req.callid = AmSession::getNewId();

  initFromLocalRequest(n_req);

  headerfilter = cp.headerfilter;
  reply_translations = cp.reply_translations;
  append_headers = cp.append_headers_req;
  keep_vias = cp.keep_vias;

  setContact(cp.bleg_contact);

  if(cp.auth_enabled) {
    // adding auth handler
    AmDynInvokeFactory* uac_auth_f =
      AmPlugIn::instance()->getFactory4Di("uac_auth");
    if (NULL == uac_auth_f)  {
      ERROR("uac_auth module not loaded. uac auth NOT enabled.\n");
    } else {
      AmDynInvoke* uac_auth_i = uac_auth_f->getInstance();

      // get a sessionEventHandler from uac_auth
      AmArg di_args,ret;
      AmArg cred_h,dlg_ctrl;

      CredentialHolder* c = (CredentialHolder*)this;
      cred_h.setBorrowedPointer(c);

      DialogControl* cc = (DialogControl*)this;
      dlg_ctrl.setBorrowedPointer(cc);

      di_args.push(cred_h);
      di_args.push(dlg_ctrl);

      auth_cred.reset(new UACAuthCred(cp.auth_credentials));
      uac_auth_i->invoke("getHandler", di_args, ret);

      if (!ret.size()) {
        ERROR("Can not add auth handler to new registration!\n");
      } else {
        AmObject* p = ret.get(0).asObject();
        if (p != NULL) {
          AmSessionEventHandler* h = dynamic_cast<AmSessionEventHandler*>(p);
          if (h != NULL) {
            auth_h.reset(h);
            DBG("uac auth enabled for callee session.\n");
          }
        }
      }
    }
  }

  return 0;
}

int SimpleRelayDialog::initUAS(const AmSipRequest& req, 
			       const SBCCallProfile& cp)
{
  for (list<CCModuleInfo>::iterator i = cc_ext.begin(); i != cc_ext.end(); ++i) {
    i->module->initUAS(req, i->user_data);
  }
  setLocalTag(AmSession::getNewId());

  AmEventDispatcher* ev_disp = AmEventDispatcher::instance();
  if(!ev_disp->addEventQueue(local_tag,this)) {
    ERROR("addEventQueue(%s,%p) failed.\n",
	  local_tag.c_str(),this);
    return -1;
  }

  ParamReplacerCtx ctx(&cp);
  if(cp.apply_a_routing(ctx,req,*this) < 0)
    return -1;

  headerfilter = cp.headerfilter;
  reply_translations = cp.reply_translations;
  append_headers = cp.aleg_append_headers_req;
  transparent_dlg_id = cp.transparent_dlg_id;
  keep_vias = cp.bleg_keep_vias;
  fix_replaces_ref = cp.fix_replaces_ref=="yes";

  setContactParams(cp.dlg_contact_params);

  return 0;
}

/**
 * SBCSimpleRelay
 */

int SBCSimpleRelay::start(const SimpleRelayCreator::Relay& relay,
			  const AmSipRequest& req, const SBCCallProfile& cp)
{
  assert(relay.first);
  assert(relay.second);

  // set auto-destruction on finalize()
  relay.first->setParent(relay.first);
  relay.second->setParent(relay.second);

  AmSipRequest n_req(req);

  if (!cp.append_headers.empty()) {
    n_req.hdrs += cp.append_headers;
  }

  if(relay.first->initUAS(n_req,cp)
     || relay.second->initUAC(n_req,cp)) {

    relay.first->finalize();
    relay.second->finalize();
    return 0;
  }

  relay.first->setOtherDlg(relay.second->getLocalTag());
  relay.second->setOtherDlg(relay.first->getLocalTag());

  relay.first->onRxRequest(n_req);
  if(relay.first->terminated()) {
    // free memory
    relay.first->finalize();
    relay.second->finalize();
    // avoid the caller to reply with 500
    // as the request should have been replied
    // already from within updateStatus(req)
    return 0;
  }

  // must be added to the same worker thread
  EventQueueWorker* worker = SBCFactory::instance()->
    subnot_processor.getWorker();
  if(!worker) return -1;

  worker->startEventQueue(relay.first);
  worker->startEventQueue(relay.second);
  
  return 0;
}
