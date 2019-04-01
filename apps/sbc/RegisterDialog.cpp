#include "RegisterDialog.h"
#include "AmSipHeaders.h"
#include "arg_conversion.h"
#include "sip/parse_nameaddr.h"
#include "sip/parse_common.h"
#include "RegisterCache.h"
#include "AmSession.h"
#include "AmConfig.h"

#include <algorithm>
using std::make_pair;

#define DEFAULT_REG_EXPIRES 3600

RegisterDialog::RegisterDialog(SBCCallProfile &profile, vector<AmDynInvoke*> &cc_modules)
  : SimpleRelayDialog(profile, cc_modules),
    star_contact(false),
    contact_hiding(false),
    reg_caching(false),
    max_ua_expire(0),
    min_reg_expire(0)
{
}

RegisterDialog::~RegisterDialog()
{
}

int RegisterDialog::parseContacts(const string& contacts, vector<AmUriParser>& cv)
{
  list<cstring> contact_list;
  
  DBG("parsing contacts: '%s'\n",contacts.c_str());

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

int RegisterDialog::replyFromCache(const AmSipRequest& req)
{
  struct timeval now;
  gettimeofday(&now,NULL);
  RegisterCache::instance();

  // for each contact, set 'expires' correctly:
  // - either original expires, if <= max_ua_expire
  //   or max_ua_expire
  string contact_hdr = SIP_HDR_COLSP(SIP_HDR_CONTACT);
  for(map<string,AmUriParser>::iterator contact_it = alias_map.begin();
      contact_it != alias_map.end(); contact_it++) {

    long int expires;
    AmUriParser& contact = contact_it->second;

    if(!str2long(contact.params["expires"], expires)) {
      ERROR("failed to parse contact-expires for the second time\n");
      reply_error(req, 500, "Server internal error", "", logger);
      return -1;
    }

    if(max_ua_expire && (expires > (long int)max_ua_expire)) {
      contact.params["expires"] = int2str(max_ua_expire);
    }

    if(contact_it != alias_map.begin())
      contact_hdr += ", ";
    contact_hdr += contact.print();
  }
  contact_hdr += CRLF;

  // send 200 reply
  return reply_error(req, 200, "OK", contact_hdr, logger);
}

void RegisterDialog::fillAliasMap()
{
  map<string,string> aor_alias_map;
  RegisterCache::instance()->getAorAliasMap(aor,aor_alias_map);
  
  for(map<string,string>::iterator it = aor_alias_map.begin();
      it != aor_alias_map.end(); ++it) {
    AmUriParser& uri = alias_map[it->first];
    uri.uri = it->second;
    uri.parse_uri();
  }
}

int RegisterDialog::fixUacContacts(const AmSipRequest& req)
{
  // move Expires as separate header to contact parameter
  string expires = getHeader(req.hdrs, "Expires");
  unsigned int requested_expires=0;
  if (!expires.empty()) {

    if (str2i(expires, requested_expires)) {
      reply_error(req, 400, "Bad Request",
		  "Warning: Malformed expires\r\n",
                  logger);
      return -1;
    }

    if(star_contact && (requested_expires != 0)) {
      reply_error(req, 400, "Bad Request",
		  "Warning: Expires not equal 0\r\n",
                  logger);
      return -1;
    }
  }

  if(!star_contact) {
    if(!expires.empty()) {
      // adjust 'expires' from header field according to min value
      if(requested_expires && (requested_expires < min_reg_expire)) {
	requested_expires = min_reg_expire;
      }
    }
    else {
      if(min_reg_expire)
	requested_expires = min_reg_expire;
      else
	requested_expires = DEFAULT_REG_EXPIRES;
    }

    bool is_a_dereg = false;
    bool reg_cache_reply = true;
    RegisterCache* reg_cache = RegisterCache::instance();

    vector<pair<string, long int> > alias_updates;
    for(vector<AmUriParser>::iterator contact_it = uac_contacts.begin();
	contact_it != uac_contacts.end(); contact_it++) {

      long int contact_expires=0;
      map<string, string>::iterator expires_it = 
	contact_it->params.find("expires");

      if(expires_it == contact_it->params.end()) {
	// no 'expires=xxx' param, use header field or min value
	if(!is_a_dereg) is_a_dereg = !requested_expires;
	contact_it->params["expires"] = int2str(requested_expires);
	contact_expires = requested_expires;
      }

      // the rest of this loop is 
      // only for register-cache support
      if(!reg_caching)
	continue;

      RegBinding reg_binding;
      const string& uri = contact_it->uri_str();

      if(!reg_cache->getAlias(aor,uri,req.remote_ip,reg_binding)) {
	DBG("no alias in cache, created one");
	reg_binding.alias = _RegisterCache::compute_alias_hash(aor,uri,
							       req.remote_ip);
      }

      alias_map[reg_binding.alias] = *contact_it;
      contact_it->uri_user = reg_binding.alias;
      contact_it->uri_param.clear();
      DBG("using alias = '%s' for aor = '%s' and uri = '%s'",
	  reg_binding.alias.c_str(),aor.c_str(),uri.c_str());

      if(expires_it != contact_it->params.end()) {
	// 'expires=xxx' present:
	if(!str2long(expires_it->second,contact_expires)) {
	  reply_error(req, 400, "Bad Request",
		      "Warning: Malformed expires\r\n",
                      logger);
	  return -1;
	}
      }

      // use existing 'expires' param if == 0 or greater than min value
      if(contact_expires && (contact_expires < (long int)min_reg_expire)) {
	// else use min value
	contact_it->params["expires"] = int2str(min_reg_expire);
      }
      else if(!contact_expires && !is_a_dereg){
	DBG("is_a_dereg = true;");
	is_a_dereg = true;
      }

      if(!reg_cache_reply)
	continue;

      if(is_a_dereg || !reg_binding.reg_expire) { // no contact with expires=0
	reg_cache_reply = false;
      }

      // Find out whether we should send the REGISTER 
      // to the registrar or not:

      struct timeval now;
      gettimeofday(&now,NULL);

      if(max_ua_expire && (contact_expires > (long int)max_ua_expire))
	contact_expires = max_ua_expire;

      DBG("min_reg_expire = %u", min_reg_expire);
      DBG("max_ua_expire = %u", max_ua_expire);
      DBG("contact_expires = %lu", contact_expires);
      DBG("reg_expires = %li", reg_binding.reg_expire - now.tv_sec);

      contact_expires += now.tv_sec;

      if(contact_expires + 4 /* 4 seconds buffer */ 
	 >= reg_binding.reg_expire) {
	reg_cache_reply = false;
	continue;
      }

      AliasEntry alias_entry;
      if(!reg_cache->findAliasEntry(reg_binding.alias, alias_entry) ||
	 (alias_entry.source_ip != req.remote_ip) ||
	 (alias_entry.source_port != req.remote_port)) {
	DBG("no alias entry or IP/port mismatch");
	reg_cache_reply = false;
	continue;
      }

      alias_updates.push_back(make_pair(reg_binding.alias,
							 contact_expires));
    }

    if(!uac_contacts.empty() && reg_caching && reg_cache_reply) {

      for(vector<pair<string, long int> >::iterator it = alias_updates.begin();
	  it != alias_updates.end(); it++) {
	if(!reg_cache->updateAliasExpires(it->first, it->second)) {
	  // alias not found ???
	  return 0; // fwd REGISTER
	}
      }

      replyFromCache(req);
      // not really an error but 
      // SBCSimpleRelay::start() would
      // else not destroy the dialog
      return -1;
    }
  }

  return 0;
}

void RegisterDialog::fixUacContactHosts(const AmSipRequest& req,
					const SBCCallProfile& cp)
{
  ParamReplacerCtx ctx(&cp);
  int oif = getOutboundIf();
  assert(oif >= 0);
  assert((size_t)outbound_interface < AmConfig::SIP_Ifs.size());

  for(unsigned int i=0; i < uac_contacts.size(); i++) {

    if(contact_hiding) {
      uac_contacts[i].uri_user = encodeUsername(uac_contacts[i],
						req,cp,ctx);
    }
    else if(!reg_caching) {
      cp.fix_reg_contact(ctx,req,uac_contacts[i]);
      continue;
    }

    // remove 'transport' param from Contact
    removeTransport(uac_contacts[i]);

    // patch host & port
    uac_contacts[i].uri_host = AmConfig::SIP_Ifs[oif].getIP();

    if(AmConfig::SIP_Ifs[oif].LocalPort == 5060)
      uac_contacts[i].uri_port.clear();
    else
      uac_contacts[i].uri_port = int2str(AmConfig::SIP_Ifs[oif].LocalPort);
      
    DBG("Patching host, port and transport for Contact-HF: host='%s';port='%s'",
	uac_contacts[i].uri_host.c_str(),uac_contacts[i].uri_port.c_str());
  }
}

int RegisterDialog::removeTransport(AmUriParser& uri)
{
  list<sip_avp*> uri_params;
  string old_params = uri.uri_param;
  const char* c = old_params.c_str();

  if(parse_gen_params(&uri_params,&c,old_params.length(),0) < 0) {

    DBG("could not parse Contact URI parameters: '%s'",
	uri.uri_param.c_str());
    free_gen_params(&uri_params);
    return -1;
  }

  // Suppress transport parameter
  // hack to suppress transport=tcp
  string new_params;
  for(list<sip_avp*>::iterator p_it = uri_params.begin();
      p_it != uri_params.end(); p_it++) {

    DBG("parsed");
    if( ((*p_it)->name.len == (sizeof("transport")-1)) &&
	!memcmp((*p_it)->name.s,"transport",sizeof("transport")-1) ) {
      continue;
    }

    if(!new_params.empty()) new_params += ";";
    new_params += c2stlstr((*p_it)->name);
    if((*p_it)->value.len) {
      new_params += "=" + c2stlstr((*p_it)->value);
    }
  }

  free_gen_params(&uri_params);
  uri.uri_param = new_params;
  return 0;
}

int RegisterDialog::initAor(const AmSipRequest& req)
{
  AmUriParser from_parser;
  size_t end_from = 0;
  if(!from_parser.parse_contact(req.from,0,end_from)) {
    DBG("error parsing AOR: '%s'\n",req.from.c_str());
    reply_error(req,400,"Bad request - bad From HF", "", logger);
    return -1;
  }

  aor = RegisterCache::canonicalize_aor(from_parser.uri_str());
  DBG("parsed AOR: '%s'",aor.c_str());

  return 0;
}

// SimpleRelayDialog interface
int RegisterDialog::initUAC(const AmSipRequest& req, const SBCCallProfile& cp)
{
  // SIP request received
  if (req.method != SIP_METH_REGISTER) {
    ERROR("unsupported method '%s'\n", req.method.c_str());
    reply_error(req,501,"Unsupported Method", "", logger);
    return -1;
  }

  DBG("contact_hiding=%i, reg_caching=%i\n",
      cp.contact_hiding, cp.reg_caching);

  contact_hiding = cp.contact_hiding;

  reg_caching = cp.reg_caching;
  if(reg_caching) {

    source_ip = req.remote_ip;
    source_port = req.remote_port;
    local_if = req.local_if;
    from_ua = getHeader(req.hdrs,"User-Agent");
    transport = req.trsp;

    min_reg_expire = cp.min_reg_expires;
    max_ua_expire = cp.max_ua_expires;

    if(initAor(req) < 0)
      return -1;
  }

  DBG("parsing contacts: '%s'\n",req.contact.c_str());
  if (req.contact == "*") {
    star_contact = true;
  }
  else if(!req.contact.empty()) {
    if (parseContacts(req.contact, uac_contacts) < 0) {
      reply_error(req, 400, "Bad Request", "Warning: Malformed contact\r\n", logger);
      return -1;
    }

    if (uac_contacts.size() == 0) {
      reply_error(req, 400, "Bad Request", "Warning: Malformed contact\r\n", logger);
      return -1;
    }
  }

  if(fixUacContacts(req) < 0)
    return -1;

  if(SimpleRelayDialog::initUAC(req,cp) < 0)
    return -1;

  if(star_contact || uac_contacts.empty()) {
    // prepare bindings to be deleted on reply
    if(reg_caching) {
      fillAliasMap();
    }
    return 0;
  }
  
  fixUacContactHosts(req,cp);

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

  // unsigned int req_expires = 0;
  // string expires_str = getHeader(req.hdrs, "Expires");
  // if (!expires_str.empty()) {
  //   str2i(expires_str, req_expires);
  // }

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
	// the registrar MUST add a 'expires' 
	// parameter to each contact
	string expires_str = it->params["expires"];
	if (!expires_str.empty()) {
	  str2i(expires_str, expires);
	}

	AmUriParser& orig_contact = alias_it->second;
	it->uri_user  = orig_contact.uri_user;
	it->uri_host  = orig_contact.uri_host;
	it->uri_port  = orig_contact.uri_port;
	it->uri_param = orig_contact.uri_param;

	// check orig expires
	string orig_expires_str = orig_contact.params["expires"];
	unsigned int orig_expires=0;
	str2i(orig_expires_str,orig_expires);

	if(!orig_expires || (expires < orig_expires)) {
	  orig_expires = expires;
	  orig_expires_str = expires_str;
	}

	if(max_ua_expire && (orig_expires > max_ua_expire)) {
	  orig_expires = max_ua_expire;
	  orig_expires_str = int2str(orig_expires);
	}
	// else {
	  // (max_ua_expire >= orig_expires > 0)
	  // or (max_ua_expire == 0)
	  // -> use the original
	// }

	it->params["expires"] = orig_expires_str;

	// Update global reg cache & alias map
	// with new 'expire' value and new entries

	AliasEntry alias_entry;
	alias_entry.aor         = aor;
	alias_entry.contact_uri = orig_contact.uri_str();
	alias_entry.alias       = alias_it->first;
	alias_entry.source_ip   = source_ip;
	alias_entry.source_port = source_port;
	alias_entry.remote_ua   = from_ua;
	alias_entry.trsp        = transport;
	alias_entry.local_if    = local_if;
	alias_entry.ua_expire   = orig_expires + now.tv_sec;

	RegisterCache* reg_cache = RegisterCache::instance();
	reg_cache->update(alias_it->first, expires + now.tv_sec, alias_entry);

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

  if(reply.code >= 200 && reply.code < 300) {
    flags |= SIP_FLAGS_NOCONTACT;
    removeHeader(reply.hdrs, SIP_HDR_EXPIRES);
    removeHeader(reply.hdrs, SIP_HDR_MIN_EXPIRES);
  }

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
