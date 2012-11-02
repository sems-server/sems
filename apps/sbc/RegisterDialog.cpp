#include "RegisterDialog.h"
#include "AmSipHeaders.h"
#include "arg_conversion.h"
#include "sip/parse_nameaddr.h"

#include "AmConfig.h"

RegisterDialog::RegisterDialog()
{
}

RegisterDialog::~RegisterDialog()
{
}

int RegisterDialog::parseContacts(const string& contacts,
				  vector<AmUriParser>& cv)
{
  list<cstring> contact_list;
  
  if(parse_nameaddr_list(contact_list, contacts.c_str(), 
			 contacts.length()) < 0) {
    DBG("Could not parse contact list\n");
    return -1;
  }
  
  size_t end;
  for(list<cstring>::iterator ct_it = contact_list.begin();
      ct_it != contact_list.end(); ct_it++) {

    AmUriParser contact;
    if (!contact.parse_contact(c2stlstr(*ct_it), 0, end)) {
      DBG("error parsing contact: '%.*s'\n",ct_it->len, ct_it->s);
      return -1;
    } else {
      DBG("successfully parsed contact %s@%s\n",
	  contact.uri_user.c_str(), 
	  contact.uri_host.c_str());
      cv.push_back(contact);
    }
  }
  
  return 0;
}


// SimpleRelayDialog interface
int RegisterDialog::initUAC(const AmSipRequest& req, const SBCCallProfile& cp)
{
  // SIP request received
  if (req.method != SIP_METH_REGISTER) {
    ERROR("unsupported method '%s'\n", req.method.c_str());
    reply_error(req,501,"Unsupported Method");
    return -1;
  }

  DBG("parsing contacts: '%s'\n",req.contact.c_str());
  if (!parseContacts(req.contact, uac_contacts) < 0) {
    reply_error(req, 400, "Bad Request", "Warning: Malformed contact\r\n");
    return -1;
  }

  // move Expires as separate header to contact parameter
  string expires = getHeader(req.hdrs, "Expires");
  if (!expires.empty()) {

    unsigned int requested_expires=0;
    if (str2i(expires, requested_expires)) {
      reply_error(req, 400, "Bad Request",
		  "Warning: Malformed expires\r\n");
      return -1;
    }

    for(vector<AmUriParser>::iterator contact_it = uac_contacts.begin();
	contact_it != uac_contacts.end(); contact_it++) {

      if(contact_it->params.find("expires") == contact_it->params.end())
	contact_it->params["expires"] = expires;
    }
  }

  // todo: limit / extend expires: UAS side

  orig_contacts = uac_contacts;
  contact_hiding = cp.contact_hiding;

  if(SimpleRelayDialog::initUAC(req,cp) < 0)
    return -1;
  
  ParamReplacerCtx ctx(&cp);
  int oif = getOutboundIf();
  assert(oif >= 0);
  assert((size_t)outbound_interface < AmConfig::Ifs.size());

  for(unsigned int i=0; i < uac_contacts.size(); i++) {

    cp.fix_reg_contact(req,ctx,uac_contacts[i]);

    uac_contacts[i].uri_user = encodeUsername(orig_contacts[i],
					      req,cp,ctx);

    uac_contacts[i].uri_host = AmConfig::Ifs[oif].LocalSIPIP;

    if(AmConfig::Ifs[oif].LocalSIPPort == 5060)
      uac_contacts[i].uri_port.clear();
    else
      uac_contacts[i].uri_port = int2str(AmConfig::Ifs[oif].LocalSIPPort);
      
    DBG("Patching host and port for Contact-HF: host='%s';port='%s'",
	uac_contacts[i].uri_host.c_str(),uac_contacts[i].uri_port.c_str());
    
    oc_map[uac_contacts[i].uri_user] = &orig_contacts[i];
  }

  return 0;
}

int RegisterDialog::initUAS(const AmSipRequest& req, const SBCCallProfile& cp)
{
  return SimpleRelayDialog::initUAS(req,cp);
}

// AmBasicSipEventHandler interface
void RegisterDialog::onSipReply(const AmSipRequest& req, const AmSipReply& reply,
				int old_dlg_status)
{
  string contacts;

  if((reply.code < 200) || (reply.code >= 300) ||
     reply.contact.empty()) {
       
    SimpleRelayDialog::onSipReply(req,reply,old_dlg_status);
    return;
  }

  DBG("parsing server contact set '%s'\n", reply.contact.c_str());
  vector<AmUriParser> uas_contacts;
  parseContacts(reply.contact,uas_contacts);

  DBG("Got %zd server contacts\n", uas_contacts.size());

  // find contact we tried to register
  if(contact_hiding)
    for (vector<AmUriParser>::iterator it =
	   uas_contacts.begin(); it != uas_contacts.end(); it++) {
      map<string,AmUriParser*>::iterator orig_it = oc_map.find(it->uri_user);
      if (orig_it != oc_map.end()) {
	// the other leg changed host:port, so compare 
	// username instead of it->isEqual(uac_contact)
	// replace with client contact
	const AmUriParser& original_contact = *orig_it->second;
	DBG("found contact we registered - replacing with original %s@%s:%s\n",
	    original_contact.uri_user.c_str(), original_contact.uri_host.c_str(),
	    original_contact.uri_port.c_str());
	
	it->display_name = original_contact.display_name;
	it->uri_user = original_contact.uri_user;
	it->uri_host = original_contact.uri_host;
	it->uri_port = original_contact.uri_port;
	it->uri_headers = original_contact.uri_headers;
      }
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
      
  SimpleRelayDialog::onSipReply(req,relay_reply,old_dlg_status);
  return;
}

int RegisterDialog::onTxReply(const AmSipRequest& req, AmSipReply& reply, 
			      int& flags)
{
  DBG("code = %i; hdrs = '%s'\n", reply.code, reply.hdrs.c_str());

  if(reply.code >= 200 && reply.code < 300)
    flags |= SIP_FLAGS_NOCONTACT;

  return AmBasicSipDialog::onTxReply(req,reply,flags);
}

int RegisterDialog::onTxRequest(AmSipRequest& req, int& flags)
{
  DBG("method = %s; hdrs = '%s'\n",req.method.c_str(),req.hdrs.c_str());

  string contact;
  if (uac_contacts.size()) {
    vector<AmUriParser>::iterator it = uac_contacts.begin();
    contact = it->print();
    it++;
    while (it != uac_contacts.end()) {
      contact += ", " + it->print();
      it++;
    }
  }

  DBG("generated new contact: '%s'\n", contact.c_str());
  req.hdrs += SIP_HDR_COLSP(SIP_HDR_CONTACT) + contact + CRLF;
  flags |= SIP_FLAGS_NOCONTACT;

  return AmBasicSipDialog::onTxRequest(req,flags);
}

string RegisterDialog::encodeUsername(const AmUriParser& original_contact,
				      const AmSipRequest& req,
				      const SBCCallProfile& cp,
				      ParamReplacerCtx& ctx)
{
  AmArg ch_dict;
  ch_dict["u"] = original_contact.uri_user;
  ch_dict["h"] = original_contact.uri_host;
  ch_dict["p"] = original_contact.uri_port;
  
  string contact_hiding_prefix =
    ctx.replaceParameters(cp.contact_hiding_prefix, "CH prefix", req);
  
  string contact_hiding_vars =
    ctx.replaceParameters(cp.contact_hiding_vars, "CH vars", req);
  
  // ex contact_hiding_vars si=10.0.0.1;st=tcp
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
  return contact_hiding_prefix + encoded;
}

bool RegisterDialog::decodeUsername(const string& encoded_user, AmSipRequest& req)
{
  DBG("trying to decode hidden contact variables from '%s'\n", 
      encoded_user.c_str());

  AmArg vars;
  if (!username2arg(encoded_user, vars)) {
    DBG("decoding failed!\n");
    return false;
  }
  DBG("decoded variables: '%s'\n", AmArg::print(vars).c_str());

  if(!vars.hasMember("u") || !isArgCStr(vars["u"]) ||
     !vars.hasMember("h") || !isArgCStr(vars["h"]) ||
     !vars.hasMember("p") || !isArgCStr(vars["p"]) ) {
    
    DBG("missing variables or type mismatch!\n");
    return false;
  }
  
  AmUriParser ruri_parser;
  ruri_parser.uri = req.r_uri;
  if (!ruri_parser.parse_uri()) {
    DBG("Error parsing R-URI '%s'\n", ruri_parser.uri.c_str());
    return false;
  }
  else {
    ruri_parser.uri_user = vars["u"].asCStr();
    ruri_parser.uri_host = vars["h"].asCStr();
    ruri_parser.uri_port = vars["p"].asCStr();
    req.r_uri = ruri_parser.uri_str();
  }

  return true;
}
