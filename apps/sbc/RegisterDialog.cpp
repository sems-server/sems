#include "RegisterDialog.h"
#include "AmSipHeaders.h"
#include "arg_conversion.h"

RegisterDialog::RegisterDialog()
{
}

RegisterDialog::~RegisterDialog()
{
}

// SimpleRelayDialog interface
int RegisterDialog::initUAC(const AmSipRequest& req, const SBCCallProfile& cp)
{
  return SimpleRelayDialog::initUAC(req,cp);
}

int RegisterDialog::initUAS(const AmSipRequest& req, const SBCCallProfile& cp)
{
  // SIP request received
  if (req.method != SIP_METH_REGISTER) {
    ERROR("unsupported method '%s'\n", req.method.c_str());
    reply(req,501,"Unsupported Method");
    return -1;
  }

  size_t end;
  if (!uac_contact.parse_contact(req.contact, 0, end)) {
    reply(req, 400, "Bad Request", "", "", "Warning: Malformed contact\r\n");
    //status = Failed;
    return -1;
  }

  // move Expires as separate header to contact parameter
  string expires = getHeader(req.hdrs, "Expires");
  if (!expires.empty()) {
    uac_contact.params["expires"] = expires;
  }

  unsigned int requested_expires=0;
  if (str2i(uac_contact.params["expires"], requested_expires)) {
    reply(req, 400, "Bad Request", "", "", "Warning: Malformed expires\r\n");
    //status = Failed;
    return -1;
  }

  // todo: limit / extend expires: UAS side

  original_contact = uac_contact;
  
  ParamReplacerCtx ctx(&cp);

  if (!cp.contact_displayname.empty()) {
    uac_contact.display_name = 
      ctx.replaceParameters(cp.contact_displayname, "Contact DN", req);
  }
  if (!cp.contact_user.empty()) {
    uac_contact.uri_user = 
      ctx.replaceParameters(cp.contact_user, "Contact User", req);
  }
  if (!cp.contact_host.empty()) {
    uac_contact.uri_host = 
      ctx.replaceParameters(cp.contact_host, "Contact host", req);
  }
  if (!cp.contact_port.empty()) {
    uac_contact.uri_port =
      ctx.replaceParameters(cp.contact_port, "Contact port", req);
  }

  if (cp.contact_hiding) {
    // todo: optimize!
    AmArg ch_dict;
    ch_dict["u"] = original_contact.uri_user;
    ch_dict["h"] = original_contact.uri_host;
    ch_dict["p"] = original_contact.uri_port;

    string contact_hiding_prefix =
      ctx.replaceParameters(cp.contact_hiding_prefix, "CH prefix", req);

    string contact_hiding_vars =
      ctx.replaceParameters(cp.contact_hiding_vars, "CH vars", req);

    //    ex contact_hiding_vars si=10.0.0.1;sp=5060;st=tcp
    if (!contact_hiding_vars.empty()) {
      vector<string> ve = explode(contact_hiding_vars, ";");
      for (vector<string>::iterator it=ve.begin(); it!=ve.end(); it++) {
	vector<string> e = explode(*it, "=");
	if (e.size()==2)
	  ch_dict[e[0]]=e[1];
      }
    }
    string encoded = arg2username(ch_dict);
    DBG("contact variables: '%s'\n", encoded.c_str());
    uac_contact.uri_user = contact_hiding_prefix + encoded;
  }

  return SimpleRelayDialog::initUAS(req,cp);
}

// AmBasicSipEventHandler interface
void RegisterDialog::onSipRequest(const AmSipRequest& req)
{
  AmSipRequest relay_req(req);

  removeHeader(relay_req.hdrs,"Expires");
  relay_req.contact = uac_contact.print();
  DBG("Original Contact: '%s'\n", req.contact.c_str());
  DBG("New Contact: '%s'\n", relay_req.contact.c_str());

  relay_req.hdrs += SIP_HDR_COLSP(SIP_HDR_CONTACT) + relay_req.contact + CRLF;

  SimpleRelayDialog::onSipRequest(relay_req);
}

void RegisterDialog::onSipReply(const AmSipRequest& req, const AmSipReply& reply,
				int old_dlg_status)
{
  SimpleRelayDialog::onSipReply(req,reply,old_dlg_status);
}

void RegisterDialog::onB2BReply(const AmSipReply& reply)
{
  string contacts;

  if((reply.code < 200) || (reply.code >= 300) ||
     reply.contact.empty()) {
       
    SimpleRelayDialog::onB2BReply(reply);
    return;
  }

  DBG("parsing server contact set '%s'\n", reply.contact.c_str());
  vector<AmUriParser> uas_contacts;
  size_t pos = 0;
  while (pos < reply.contact.size()) {
    uas_contacts.push_back(AmUriParser());
    if (!uas_contacts.back().parse_contact(reply.contact, pos, pos)) {
      DBG("error parsing server contact from pos %zd in '%s'\n",
	  pos, reply.contact.c_str());
      uas_contacts.pop_back();
    } else {
      DBG("successfully parsed contact %s@%s\n",
	  uas_contacts.back().uri_user.c_str(), 
	  uas_contacts.back().uri_host.c_str());
    }
  }
      
  DBG("Got %zd server contacts\n", uas_contacts.size());

  // find contact we tried to register
  for (vector<AmUriParser>::iterator it =
	 uas_contacts.begin(); it != uas_contacts.end(); it++) {
    if (it->uri_user == uac_contact.uri_user) { 
      // the other leg changed host:port, so compare 
      // username instead of it->isEqual(uac_contact)
      // replace with client contact
      DBG("found contact we registered - replacing with original %s@%s:%s\n",
	  original_contact.uri_user.c_str(), original_contact.uri_host.c_str(),
	  original_contact.uri_port.c_str());

      it->display_name = original_contact.display_name;
      it->uri_user = original_contact.uri_user;
      it->uri_host = original_contact.uri_host;
      it->uri_port = original_contact.uri_port;
      it->uri_headers = original_contact.uri_headers;
      break;
    }
  }
      
  if (uas_contacts.size()) {
    vector<AmUriParser>::iterator it = uas_contacts.begin();
    contacts = it->print();
    it++;
    while (it != uas_contacts.end()) {
      contacts += ", " + it->print();
      it++;
    }
  }
  DBG("generated new contacts: '%s'\n", contacts.c_str());

  AmSipReply relay_reply(reply);
  relay_reply.hdrs += SIP_HDR_COLSP(SIP_HDR_CONTACT) + contacts + CRLF;
      
  SimpleRelayDialog::onB2BReply(relay_reply);
  return;
}

void RegisterDialog::onSendReply(const AmSipRequest& req, unsigned int  code,
				 const string& reason, const string& content_type,
				 const string& body, string& hdrs, int& flags)
{
  DBG("code = %i; hdrs = '%s'\n", code, hdrs.c_str());

  if(code >= 200 && code < 300)
    flags |= SIP_FLAGS_NOCONTACT;

  AmBasicSipDialog::onSendReply(req,code,hdrs,flags);
}

void RegisterDialog::onSendRequest(const string& method, 
				   const string& content_type,
				   const string& body, string& hdrs, 
				   int& flags, unsigned int cseq)
{
  DBG("method = %s; hdrs = '%s'\n",method.c_str(),hdrs.c_str());

  if(method == SIP_METH_REGISTER) {
    flags |= SIP_FLAGS_NOCONTACT;
  }
}
