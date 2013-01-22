#include "RegisterDialog.h"
#include "AmSipHeaders.h"
#include "arg_conversion.h"
#include "sip/parse_nameaddr.h"
#include "RegisterCache.h"
#include "AmSession.h"
#include "AmConfig.h"

#include <algorithm>

RegisterDialog::RegisterDialog()
  : contact_hiding(false),
    reg_caching(false),
    star_contact(false)
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

  contact_hiding = cp.contact_hiding;

  reg_caching = cp.reg_caching;
  if(reg_caching) {
    source_ip = req.remote_ip;
    source_port = req.remote_port;
    local_if = req.local_if;
  }

  AmUriParser from_parser;
  size_t end_from = 0;
  if(!from_parser.parse_contact(req.from,0,end_from)) {
    DBG("error parsing AOR: '%s'\n",req.from.c_str());
    reply_error(req,400,"Bad request - bad From HF");
    return -1;
  }

  aor = RegisterCache::canonicalize_aor(from_parser.uri_str());
  DBG("parsed AOR: '%s'",aor.c_str());

  DBG("parsing contacts: '%s'\n",req.contact.c_str());
  if (req.contact == "*") {
    star_contact = true;
  }
  else if(!req.contact.empty()) {
    if (parseContacts(req.contact, uac_contacts) < 0) {
      reply_error(req, 400, "Bad Request", "Warning: Malformed contact\r\n");
      return -1;
    }

    if (uac_contacts.size() == 0) {
      reply_error(req, 400, "Bad Request", "Warning: Malformed contact\r\n");
      return -1;
    }
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

    if(!star_contact) {
      for(vector<AmUriParser>::iterator contact_it = uac_contacts.begin();
	  contact_it != uac_contacts.end(); contact_it++) {
	
	if(contact_it->params.find("expires") == contact_it->params.end())
	  contact_it->params["expires"] = expires;
      }
    }
    else if(requested_expires != 0) {
      reply_error(req, 400, "Bad Request",
		  "Warning: Expires not equal 0\r\n");
      return -1;
    }
  }

  // todo: limit / extend expires: UAS side

  if(SimpleRelayDialog::initUAC(req,cp) < 0)
    return -1;

  if(star_contact) {

    // prepare bindings to be deleted on reply
    if(reg_caching) {
      map<string,string> aor_alias_map =
	RegisterCache::instance()->getAorAliasMap(aor);

      for(map<string,string>::iterator it = aor_alias_map.begin();
	  it != aor_alias_map.end(); ++it) {
	AmUriParser& uri = alias_map[it->first];
	uri.uri = it->second;
	uri.parse_uri();
      }
    }
    return 0;
  }
  
  ParamReplacerCtx ctx;
  int oif = getOutboundIf();
  assert(oif >= 0);
  assert((size_t)outbound_interface < AmConfig::SIP_Ifs.size());

  for(unsigned int i=0; i < uac_contacts.size(); i++) {

    if(contact_hiding) {
      uac_contacts[i].uri_user = encodeUsername(uac_contacts[i],
						req,cp,ctx);
      
      // hack to suppress transport=tcp (just hack, params like
      // "xtransport" or values like "tcpip" will be partly removed as well)
      // string params(uac_contacts[i].uri_param);
      // std::transform(params.begin(), params.end(), params.begin(), ::tolower);
      // DBG("params: %s",params.c_str());
      // size_t t_pos = params.find("transport=tcp");
      // if (t_pos != string::npos) {
      // 	uac_contacts[i].uri_param.erase(t_pos, sizeof("transport=tcp")-1);
      // 	if((t_pos < uac_contacts[i].uri_param.length()) && 
      // 	   (uac_contacts[i].uri_param[t_pos] == ';')) {
      // 	  uac_contacts[i].uri_param.erase(t_pos, 1);
      // 	}
      // }
    }
    else if(reg_caching) {
      const string& uri = uac_contacts[i].uri_str();

      RegisterCache* reg_cache = RegisterCache::instance();
      string alias = reg_cache->getAlias(aor,uri);
      if(alias.empty()) {
	alias = AmSession::getNewId();
	DBG("no alias in cache, created one");
      }

      alias_map[alias] = uac_contacts[i];
      uac_contacts[i].uri_user = alias;
      uac_contacts[i].uri_param.clear();
      DBG("using alias = '%s' for aor = '%s' and uri = '%s'",
	  alias.c_str(),aor.c_str(),uri.c_str());
    }
    else {
      cp.fix_reg_contact(ctx,req,uac_contacts[i]);
      continue;
    }

    // patch host & port
    uac_contacts[i].uri_host = AmConfig::SIP_Ifs[oif].LocalIP;

    if(AmConfig::SIP_Ifs[oif].LocalPort == 5060)
      uac_contacts[i].uri_port.clear();
    else
      uac_contacts[i].uri_port = int2str(AmConfig::SIP_Ifs[oif].LocalPort);
      
    DBG("Patching host and port for Contact-HF: host='%s';port='%s'",
	uac_contacts[i].uri_host.c_str(),uac_contacts[i].uri_port.c_str());
  }

  // patch initial CSeq to fix re-REGISTER with transparent-id enabled
  cseq = req.cseq;

  return 0;
}

// AmBasicSipEventHandler interface
void RegisterDialog::onSipReply(const AmSipRequest& req,
				const AmSipReply& reply, 
				AmBasicSipDialog::Status old_dlg_status)
{
  string contacts;

  if((reply.code < 200) || (reply.code >= 300) ||
     (uac_contacts.empty() && !reg_caching)) {

    SimpleRelayDialog::onSipReply(req,reply,old_dlg_status);
    return;
  }

  unsigned int req_expires = 0;
  string expires_str = getHeader(req.hdrs, "Expires");
  if (!expires_str.empty()) {
    str2i(expires_str, req_expires);
  }

  DBG("parsing server contact set '%s'\n", reply.contact.c_str());
  vector<AmUriParser> uas_contacts;
  parseContacts(reply.contact,uas_contacts);

  DBG("Got %zd server contacts\n", uas_contacts.size());

  // decode contacts
  if(contact_hiding || reg_caching) {

    struct timeval now;
    gettimeofday(&now,NULL);

    for (vector<AmUriParser>::iterator it =
	   uas_contacts.begin(); it != uas_contacts.end(); it++) {

      if(contact_hiding) {
	decodeUsername(it->uri_user,*it);
      }
      else if(reg_caching) {
	map<string,AmUriParser>::iterator alias_it = alias_map.find(it->uri_user);
	if(alias_it == alias_map.end()) {
	  DBG("no alias found for '%s'",it->uri_user.c_str());
	  continue;
	}

	unsigned int expires=0;
	if(it->params.find("expires") == it->params.end()) {
	  expires = req_expires;
	}
	else {
	  expires_str = alias_it->second.params["expires"];
	  if (!expires_str.empty()) {
	    str2i(expires_str, expires);
	  }
	}

	const AmUriParser& orig_contact = alias_it->second;
	it->uri_user  = orig_contact.uri_user;
	it->uri_host  = orig_contact.uri_host;
	it->uri_port  = orig_contact.uri_port;
	it->uri_param = orig_contact.uri_param;

	// Update global reg cache & alias map
	// with new 'expire' value and new entries

	AliasEntry alias_entry;
	alias_entry.contact_uri = orig_contact.uri_str();
	alias_entry.source_ip   = source_ip;
	alias_entry.source_port = source_port;
	alias_entry.local_if    = local_if;

	RegisterCache* reg_cache = RegisterCache::instance();
	reg_cache->update(aor, alias_it->first, 
			  expires + now.tv_sec, 
			  alias_entry);

	alias_map.erase(alias_it);
      }
    }

    DBG("reg_caching=%i; alias_map.empty()=%i",
	reg_caching, alias_map.empty());

    if(reg_caching && !alias_map.empty()) {
      for(map<string,AmUriParser>::iterator alias_it = alias_map.begin();
	  alias_it != alias_map.end(); ++alias_it) {
	
	// search for missing Contact-URI
	// and remove the binding
	RegisterCache* reg_cache = RegisterCache::instance();
	string contact_uri = alias_it->second.uri_str();
	DBG("removing '%s' -> '%s'",
	    contact_uri.c_str(),alias_it->first.c_str());

	reg_cache->remove(aor,contact_uri,alias_it->first);
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
  
  AmSipReply relay_reply(reply);
  if(!contacts.empty()) {
    DBG("generated new contacts: '%s'\n", contacts.c_str());
    relay_reply.hdrs += SIP_HDR_COLSP(SIP_HDR_CONTACT) + contacts + CRLF;
  }
      
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

    DBG("generated new contact: '%s'\n", contact.c_str());
    removeHeader(req.hdrs, SIP_HDR_EXPIRES);
    req.hdrs += SIP_HDR_COLSP(SIP_HDR_CONTACT) + contact + CRLF;
  }
  else if(star_contact) {
    DBG("generated new contact: '*'\n");
    req.hdrs += SIP_HDR_COLSP(SIP_HDR_CONTACT) "*" CRLF;
  }

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
    ctx.replaceParameters(cp.contact.hiding_prefix, "CH prefix", req);
  
  string contact_hiding_vars =
    ctx.replaceParameters(cp.contact.hiding_vars, "CH vars", req);
  
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

bool RegisterDialog::decodeUsername(const string& encoded_user, AmUriParser& uri)
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
  
  uri.uri_user = vars["u"].asCStr();
  uri.uri_host = vars["h"].asCStr();
  uri.uri_port = vars["p"].asCStr();

  return true;
}
