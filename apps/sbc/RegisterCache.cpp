#include "RegisterCache.h"
#include "sip/hash.h"
#include "sip/parse_uri.h"

#include "AmBasicSipDialog.h"
#include "AmSipHeaders.h"
#include "AmUriParser.h"
#include "RegisterDialog.h"
#include "AmSession.h" //getNewId
#include "AmUtils.h"
#include "SBCEventLog.h"

#include <utility>
using std::pair;
using std::make_pair;

#define REG_CACHE_CYCLE 10L /* 10 seconds to expire all buckets */

 /* in us */
#define REG_CACHE_SINGLE_CYCLE \
  ((REG_CACHE_CYCLE*1000000L)/REG_CACHE_TABLE_ENTRIES)

static unsigned int hash_1str(const string& str)
{
  unsigned int h=0;
  h = hashlittle(str.c_str(),str.length(),h);
  return h & (REG_CACHE_TABLE_ENTRIES-1);
}

static unsigned int hash_2str_1int(const string& str1, const string& str2,
				   unsigned int i)
{
  unsigned int h=i;
  h = hashlittle(str1.c_str(),str1.length(),h);
  h = hashlittle(str2.c_str(),str2.length(),h);
  return h & (REG_CACHE_TABLE_ENTRIES-1);
}

static string unescape_sip(const string& str)
{
  // TODO
  return str;
}

AorEntry* AorBucket::get(const string& aor)
{
  value_map::iterator it = find(aor);
  if(it == elmts.end())
    return NULL;
  
  return it->second;
}

void AorBucket::dump_elmt(const string& aor, 
			  const AorEntry* p_aor_entry) const
{
  DBG("'%s' ->", aor.c_str());
  if(!p_aor_entry) return;

  for(AorEntry::const_iterator it = p_aor_entry->begin();
      it != p_aor_entry->end(); it++) {

    if(it->second) {
      const RegBinding* b = it->second;
      DBG("\t'%s' -> '%s'", it->first.c_str(),
	  b ? b->alias.c_str() : "NULL");
    }
  }
}

void AorBucket::gbc(RegCacheStorageHandler* h, long int now, 
		    list<string>& alias_list)
{
  for(value_map::iterator it = elmts.begin(); it != elmts.end();) {

    AorEntry* aor_e = it->second;
    if(aor_e) {

      for(AorEntry::iterator reg_it = aor_e->begin();
	  reg_it != aor_e->end();) {

	RegBinding* binding = reg_it->second;

	if(binding && (binding->reg_expire <= now)) {

	  alias_list.push_back(binding->alias);
	  AorEntry::iterator del_it = reg_it++;

	  DBG("delete binding: '%s' -> '%s' (%li <= %li)",
	      del_it->first.c_str(),binding->alias.c_str(),
	      binding->reg_expire,now);

	  if(h) h->onDelete(it->first,del_it->first,binding->alias);
	  delete binding;
	  aor_e->erase(del_it);
	  continue;
	}
	reg_it++;
      }
    }
    if(!aor_e || aor_e->empty()) {
      DBG("delete empty AOR: '%s'", it->first.c_str());
      value_map::iterator del_it = it++;
      elmts.erase(del_it);
      continue;
    }
    it++;
  }
}

AliasEntry* AliasBucket::getContact(const string& alias)
{
  value_map::iterator it = find(alias);
  if(it == elmts.end())
    return NULL;

  return it->second;
}

void AliasEntry::fire()
{
  AmArg ev;
  ev["aor"]      = aor;
  ev["to"]       = aor;
  ev["contact"]  = contact_uri;
  ev["source"]   = source_ip;
  ev["src_port"] = source_port;
  ev["from-ua"]  = remote_ua;

  DBG("Alias expired (UA/%li): '%s' -> '%s'\n",
      (long)(AmAppTimer::instance()->unix_clock.get() - ua_expire),
      alias.c_str(),aor.c_str());

  SBCEventLog::instance()->logEvent(alias,"ua-reg-expired",ev);
}

void AliasBucket::dump_elmt(const string& alias, const AliasEntry* p_ae) const
{
  DBG("'%s' -> '%s'", alias.c_str(),
      p_ae ? p_ae->contact_uri.c_str() : "NULL");
}

string ContactBucket::getAlias(const string& contact_uri,
			       const string& remote_ip,
			       unsigned short remote_port)
{
  string key = contact_uri + "/" + remote_ip + ":" + int2str(remote_port);

  value_map::iterator it = find(key);
  if(it == elmts.end())
    return string();

  return *it->second;
}

void ContactBucket::remove(const string& contact_uri, const string& remote_ip,
			   unsigned short remote_port)
{
  string k = contact_uri + "/" + remote_ip + ":" + int2str(remote_port);
  elmts.erase(k);
}

void ContactBucket::dump_elmt(const string& key, const string* alias) const
{
  DBG("'%s' -> %s", key.c_str(), alias? alias->c_str() : "NULL");
}

struct RegCacheLogHandler
  : RegCacheStorageHandler
{
  void onDelete(const string& aor, const string& uri, const string& alias) {
    DBG("delete: aor='%s';uri='%s';alias='%s'",
	aor.c_str(),uri.c_str(),alias.c_str());
  }

  void onUpdate(const string& canon_aor, const string& alias, 
		long int expires, const AliasEntry& alias_update) {
    DBG("update: aor='%s';alias='%s';expires=%li",
	canon_aor.c_str(),alias.c_str(),expires);
  }

  void onUpdate(const string& alias, long int ua_expires) {
    DBG("update: alias='%s';ua_expires=%li",
	alias.c_str(),ua_expires);
  }
};


_RegisterCache::_RegisterCache()
  : reg_cache_ht(REG_CACHE_TABLE_ENTRIES),
    id_idx(REG_CACHE_TABLE_ENTRIES),
    contact_idx(REG_CACHE_TABLE_ENTRIES)
{
  // debug register cache WRITE operations
  setStorageHandler(new RegCacheLogHandler());
}

_RegisterCache::~_RegisterCache()
{
  DBG("##### REG CACHE DUMP #####");
  reg_cache_ht.dump();
  DBG("##### ID IDX DUMP #####");
  id_idx.dump();
  DBG("##### CONTACT IDX DUMP #####");
  contact_idx.dump();
  DBG("##### DUMP END #####");
}

void _RegisterCache::gbc(unsigned int bucket_id)
{
  // if(!bucket_id) {
  //   DBG("REG CACHE GBC CYCLE starting...");
  // }

  struct timeval now;
  gettimeofday(&now,NULL);

  AorBucket* bucket = reg_cache_ht.get_bucket(bucket_id);
  bucket->lock();
  list<string> alias_list;
  bucket->gbc(storage_handler.get(),now.tv_sec,alias_list);
  for(list<string>::iterator it = alias_list.begin();
      it != alias_list.end(); it++){
    removeAlias(*it,true);
  }
  bucket->unlock();
}

void _RegisterCache::on_stop()
{
  running.set(false);
}

void _RegisterCache::run()
{
  struct timespec tick,rem;
  tick.tv_sec  = (REG_CACHE_SINGLE_CYCLE/1000000L);
  tick.tv_nsec = (REG_CACHE_SINGLE_CYCLE - (tick.tv_sec)*1000000L) * 1000L;

  running.set(true);

  gbc_bucket_id = 0;
  while(running.get()) {
    gbc(gbc_bucket_id);
    gbc_bucket_id = (gbc_bucket_id+1);
    gbc_bucket_id &= (REG_CACHE_TABLE_ENTRIES-1);
    nanosleep(&tick,&rem);
  }  
}

/**
 * From RFC 3261 (Section 10.3, step 5):
 *  "all URI parameters MUST be removed (including the user-param), and
 *   any escaped characters MUST be converted to their unescaped form"
 */
string _RegisterCache::canonicalize_aor(const string& uri)
{
  string canon_uri;
  sip_uri parsed_uri;

  if(parse_uri(&parsed_uri,uri.c_str(),uri.length())) {
    DBG("Malformed URI: '%s'",uri.c_str());
    return "";
  }

  switch(parsed_uri.scheme) {
  case sip_uri::SIP:  canon_uri = "sip:"; break;
  case sip_uri::SIPS: canon_uri = "sips:"; break;
  default:
    DBG("Unknown URI scheme in '%s'",uri.c_str());
    return "";
  }

  if(parsed_uri.user.len) {
    canon_uri += unescape_sip(c2stlstr(parsed_uri.user)) + "@";
  }

  canon_uri += unescape_sip(c2stlstr(parsed_uri.host));

  if(parsed_uri.port != 5060) {
    canon_uri += ":" + unescape_sip(c2stlstr(parsed_uri.port_str));
  }

  return canon_uri;
}

string 
_RegisterCache::compute_alias_hash(const string& aor, const string& contact_uri,
				   const string& public_ip)
{
  unsigned int h1=0,h2=0;
  h1 = hashlittle(aor.c_str(),aor.length(),h1);
  h1 = hashlittle(contact_uri.c_str(),contact_uri.length(),h1);
  h2 = hashlittle(public_ip.c_str(),public_ip.length(),h1);

  return int2hex(h1,true) + int2hex(h2,true);
}

AorBucket* _RegisterCache::getAorBucket(const string& aor)
{
  return reg_cache_ht.get_bucket(hash_1str(aor));
}

void ContactBucket::insert(const string& contact_uri, const string& remote_ip,
			   unsigned short remote_port, const string& alias)
{
  string k = contact_uri + "/" + remote_ip + ":" + int2str(remote_port);
  elmts.insert(value_map::value_type(k,new string(alias)));
}

bool _RegisterCache::getAlias(const string& canon_aor, const string& uri,
			      const string& public_ip, RegBinding& out_binding)
{
  if(canon_aor.empty()) {
    DBG("Canonical AOR is empty");
    return false;
  }

  bool alias_found = false;
  AorBucket* bucket = getAorBucket(canon_aor);
  bucket->lock();

  AorEntry* aor_e = bucket->get(canon_aor);
  if(aor_e){
    AorEntry::iterator binding_it = aor_e->find(uri + "/" + public_ip);
    if((binding_it != aor_e->end()) && binding_it->second) {
      alias_found = true;
      out_binding = *binding_it->second;
    }
  }

  bucket->unlock();

  return alias_found;
}

AliasBucket* _RegisterCache::getAliasBucket(const string& alias)
{
  return id_idx.get_bucket(hash_1str(alias));
}

ContactBucket* _RegisterCache::getContactBucket(const string& contact_uri,
						const string& remote_ip,
						unsigned short remote_port)
{
  unsigned int h = hash_2str_1int(contact_uri,remote_ip,remote_port);
  return contact_idx.get_bucket(h);
}

void _RegisterCache::setAliasUATimer(AliasEntry* alias_e)
{
  if(!alias_e->ua_expire)
    return;

  AmAppTimer* app_timer = AmAppTimer::instance();
  time_t timeout = alias_e->ua_expire - app_timer->unix_clock.get();
  if(timeout > 0) {
    app_timer->setTimer(alias_e,timeout);
  }
  else {
    // already expired at the UA side, just fire the timer handler
    alias_e->fire();
  }
}

void _RegisterCache::removeAliasUATimer(AliasEntry* alias_e)
{
  AmAppTimer::instance()->removeTimer(alias_e);
}

void _RegisterCache::update(const string& alias, long int reg_expires,
			    const AliasEntry& alias_update)
{
  string uri = alias_update.contact_uri;
  string canon_aor = alias_update.aor;
  string public_ip = alias_update.source_ip;
  if(canon_aor.empty()) {
    ERROR("Canonical AOR is empty: could not update register cache");
    return;
  }
  if(uri.empty()) {
    ERROR("Contact-URI is empty: could not update register cache");
    return;
  }
  if(public_ip.empty()) {
    ERROR("Source-IP is empty: could not update register cache");
    return;
  }

  AorBucket* bucket = getAorBucket(canon_aor);
  AliasBucket* alias_bucket = getAliasBucket(alias);

  bucket->lock();
  alias_bucket->lock();

  // Try to get the existing binding
  RegBinding* binding = NULL;
  AorEntry* aor_e = bucket->get(canon_aor);
  if(!aor_e) {
    // insert AorEntry if none
    aor_e = new AorEntry();
    bucket->insert(canon_aor,aor_e);
    DBG("inserted new AOR '%s'",canon_aor.c_str());
  }
  else {
    string idx = uri + "/" + public_ip;
    AorEntry::iterator binding_it = aor_e->find(idx);
    if(binding_it != aor_e->end()) {
      binding = binding_it->second;
    }
  }
  
  if(!binding) {
    // insert one if none exist
    binding = new RegBinding();
    binding->alias = alias;
    aor_e->insert(AorEntry::value_type(uri + "/" + public_ip,binding));
    DBG("inserted new binding: '%s' -> '%s'",
	uri.c_str(), alias.c_str());

    ContactBucket* ct_bucket = getContactBucket(uri,alias_update.source_ip,
						alias_update.source_port);
    ct_bucket->lock();
    ct_bucket->insert(uri,alias_update.source_ip,
		      alias_update.source_port,alias);
    ct_bucket->unlock();
  }
  else {
    DBG("updating existing binding: '%s' -> '%s'",
	uri.c_str(), binding->alias.c_str());
    if(alias != binding->alias) {
      ERROR("used alias ('%s') is different from stored one ('%s')",
	    alias.c_str(), binding->alias.c_str());
    }
  }
  // and update binding
  binding->reg_expire = reg_expires;

  AliasEntry* alias_e = alias_bucket->getContact(alias);
  // if no alias map entry, insert a new one
  if(!alias_e) {
    DBG("inserting alias map entry: '%s' -> '%s'",
	alias.c_str(), uri.c_str());
    alias_e = new AliasEntry(alias_update);
    alias_bucket->insert(alias,alias_e);
  }
  else {
    *alias_e = alias_update;
  }

#if 0 // disabled UA-timer
  if(alias_e->ua_expire) {
    setAliasUATimer(alias_e);
  }
#endif
  
  if(storage_handler.get())
    storage_handler->onUpdate(canon_aor,alias,reg_expires,*alias_e);

  alias_bucket->unlock();
  bucket->unlock();
}

void _RegisterCache::update(long int reg_expires, const AliasEntry& alias_update)
{
  string uri = alias_update.contact_uri;
  string canon_aor = alias_update.aor;
  string public_ip = alias_update.source_ip;
  if(canon_aor.empty()) {
    ERROR("Canonical AOR is empty: could not update register cache");
    return;
  }
  if(uri.empty()) {
    ERROR("Contact-URI is empty: could not update register cache");
    return;
  }
  if(public_ip.empty()) {
    ERROR("Source-IP is empty: could not update register cache");
    return;
  }

  string idx = uri + "/" + public_ip;
  AorBucket* bucket = getAorBucket(canon_aor);
  bucket->lock();

  // Try to get the existing binding
  RegBinding* binding = NULL;
  AorEntry* aor_e = bucket->get(canon_aor);
  if(aor_e) {
    // take the first, as we do not expect others to be here
    AorEntry::iterator binding_it = aor_e->begin();

    if(binding_it != aor_e->end()) {

      binding = binding_it->second;
      if(binding && (binding_it->first != idx)) {

	// contact-uri and/or public IP has changed...
	string alias = binding->alias;

	AliasEntry ae;
	if(findAliasEntry(alias,ae)) {

	  // change contact index
	  ContactBucket* ct_bucket = 
	    getContactBucket(ae.contact_uri,ae.source_ip,ae.source_port);
	  ct_bucket->lock();
	  ct_bucket->remove(ae.contact_uri,ae.source_ip,ae.source_port);
	  ct_bucket->unlock();

	  ct_bucket = getContactBucket(uri,public_ip,alias_update.source_port);
	  ct_bucket->lock();
	  ct_bucket->insert(uri,public_ip,alias_update.source_port,alias);
	  ct_bucket->unlock();
	}

	// relink binding with the new index
      	aor_e->erase(binding_it);
	aor_e->insert(AorEntry::value_type(idx, binding));
      }
      else if(!binding) {
	// probably never happens, but who knows?
	aor_e->erase(binding_it);
      }
    }
  } 
  else {
    // insert AorEntry if none
    aor_e = new AorEntry();
    bucket->insert(canon_aor,aor_e);
    DBG("inserted new AOR '%s'",canon_aor.c_str());
  }
  
  if(!binding) {
    // insert one if none exist
    binding = new RegBinding();
    binding->alias = _RegisterCache::
      compute_alias_hash(canon_aor,uri,public_ip);

    string idx = uri + "/" + public_ip;
    aor_e->insert(AorEntry::value_type(idx, binding));
    DBG("inserted new binding: '%s' -> '%s'",
	idx.c_str(), binding->alias.c_str());

    ContactBucket* ct_bucket = getContactBucket(uri,alias_update.source_ip,
						alias_update.source_port);
    ct_bucket->lock();
    ct_bucket->insert(uri,alias_update.source_ip,
		      alias_update.source_port,binding->alias);
    ct_bucket->unlock();
  }
  else {
    DBG("updating existing binding: '%s' -> '%s'",
	uri.c_str(), binding->alias.c_str());
  }
  // and update binding
  binding->reg_expire = reg_expires;

  AliasBucket* alias_bucket = getAliasBucket(binding->alias);
  alias_bucket->lock();

  AliasEntry* alias_e = alias_bucket->getContact(binding->alias);
  // if no alias map entry, insert a new one
  if(!alias_e) {
    DBG("inserting alias map entry: '%s' -> '%s'",
	binding->alias.c_str(), uri.c_str());
    alias_e = new AliasEntry(alias_update);
    alias_e->alias = binding->alias;
    alias_bucket->insert(binding->alias,alias_e);
  }
  else {
    *alias_e = alias_update;
    alias_e->alias = binding->alias;
  }

#if 0 // disabled UA-timer
  if(alias_e->ua_expire) {
    setAliasUATimer(alias_e);
  }
#endif
  
  if(storage_handler.get())
    storage_handler->onUpdate(canon_aor,binding->alias,
			      reg_expires,*alias_e);

  alias_bucket->unlock();
  bucket->unlock();
}

bool _RegisterCache::updateAliasExpires(const string& alias, long int ua_expires)
{
  bool res = false;
  AliasBucket* alias_bucket = getAliasBucket(alias);
  alias_bucket->lock();

  AliasEntry* alias_e = alias_bucket->getContact(alias);
  if(alias_e) {
    alias_e->ua_expire = ua_expires;
#if 0 // disabled UA-timer
    if(alias_e->ua_expire)
      setAliasUATimer(alias_e);
#endif

    if(storage_handler.get()) {
      storage_handler->onUpdate(alias,ua_expires);
    }
    res = true;
  }

  alias_bucket->unlock();
  return res;
}

void _RegisterCache::remove(const string& canon_aor, const string& uri,
			    const string& alias)
{
  if(canon_aor.empty()) {
    DBG("Canonical AOR is empty");
    return;
  }

  AorBucket* bucket = getAorBucket(canon_aor);
  bucket->lock();

  DBG("removing entries for aor = '%s', uri = '%s' and alias = '%s'",
      canon_aor.c_str(), uri.c_str(), alias.c_str());

  AorEntry* aor_e = bucket->get(canon_aor);
  if(aor_e) {
    // remove all bindings for which the alias matches
    for(AorEntry::iterator binding_it = aor_e->begin();
	binding_it != aor_e->end();) {

      RegBinding* binding = binding_it->second;
      if(!binding || (binding->alias == alias)) {

	delete binding;
	AorEntry::iterator del_it = binding_it++;
	aor_e->erase(del_it);
	continue;
      }

      binding_it++;
    }
    if(aor_e->empty()) {
      bucket->remove(canon_aor);
    }
  }

  removeAlias(alias,false);
  bucket->unlock();
}

void _RegisterCache::remove(const string& aor)
{
  if(aor.empty()) {
    DBG("Canonical AOR is empty");
    return;
  }

  AorBucket* bucket = getAorBucket(aor);
  bucket->lock();

  DBG("removing entries for aor = '%s'", aor.c_str());

  AorEntry* aor_e = bucket->get(aor);
  if(aor_e) {
    for(AorEntry::iterator binding_it = aor_e->begin();
	binding_it != aor_e->end(); binding_it++) {

      RegBinding* binding = binding_it->second;
      if(binding) {
	removeAlias(binding->alias,false);
	delete binding;
      }
    }
    bucket->remove(aor);
  }

  bucket->unlock();
}

void _RegisterCache::removeAlias(const string& alias, bool generate_event)
{
  AliasBucket* alias_bucket = getAliasBucket(alias);
  alias_bucket->lock();

  AliasEntry* ae = alias_bucket->getContact(alias);
  if(ae) {
#if 0 // disabled UA-timer
    if(ae->ua_expire)
      removeAliasUATimer(ae);
#endif
    
    if(generate_event) {
      AmArg ev;
      ev["aor"]      = ae->aor;
      ev["to"]       = ae->aor;
      ev["contact"]  = ae->contact_uri;
      ev["source"]   = ae->source_ip;
      ev["src_port"] = ae->source_port;
      ev["from-ua"]  = ae->remote_ua;
    
      DBG("Alias expired @registrar (UA/%li): '%s' -> '%s'\n",
	  (long)(AmAppTimer::instance()->unix_clock.get() - ae->ua_expire),
	  ae->alias.c_str(),ae->aor.c_str());

      SBCEventLog::instance()->logEvent(ae->alias,"reg-expired",ev);
    }

    ContactBucket* ct_bucket = getContactBucket(ae->contact_uri,
						ae->source_ip,
						ae->source_port);
    ct_bucket->lock();
    ct_bucket->remove(ae->contact_uri,ae->source_ip,ae->source_port);
    ct_bucket->unlock();

    storage_handler->onDelete(ae->aor,
			      ae->contact_uri,
			      ae->alias);
  }
  alias_bucket->remove(alias);
  alias_bucket->unlock();
}

bool _RegisterCache::getAorAliasMap(const string& canon_aor, 
				    map<string,string>& alias_map)
{
  if(canon_aor.empty()) {
    DBG("Canonical AOR is empty");
    return false;
  }

  AorBucket* bucket = getAorBucket(canon_aor);
  bucket->lock();
  AorEntry* aor_e = bucket->get(canon_aor);
  if(aor_e) {
    for(AorEntry::iterator it = aor_e->begin();
	it != aor_e->end(); ++it) {

      if(!it->second)
	continue;

      AliasEntry ae;
      if(!findAliasEntry(it->second->alias,ae))
	continue;

      alias_map[ae.alias] = ae.contact_uri;
    }
  }
  bucket->unlock();

  return true;
}

bool _RegisterCache::findAliasEntry(const string& alias, AliasEntry& alias_entry)
{
  bool res = false;

  AliasBucket* bucket = getAliasBucket(alias);
  bucket->lock();
  
  AliasEntry* a = bucket->getContact(alias);
  if(a) {
    alias_entry = *a;
    res = true;
  }

  bucket->unlock();
  return res;
}

bool _RegisterCache::findAEByContact(const string& contact_uri,
				     const string& remote_ip,
				     unsigned short remote_port,
				     AliasEntry& ae)
{
  bool res = false;

  ContactBucket* ct_bucket = getContactBucket(contact_uri,remote_ip,
					      remote_port);
  ct_bucket->lock();
  string alias = ct_bucket->getAlias(contact_uri,remote_ip,remote_port);
  ct_bucket->unlock();

  if(alias.empty())
    return false;

  res = findAliasEntry(alias,ae);

  return res;
}


int _RegisterCache::parseAoR(RegisterCacheCtx& ctx,
			     const AmSipRequest& req,
                             msg_logger *logger)
{
  if(ctx.aor_parsed)
    return 0;

  AmUriParser from_parser;
  size_t end_from = 0;
  if(!from_parser.parse_contact(req.from,0,end_from)) {
    DBG("error parsing AoR: '%s'\n",req.from.c_str());
    AmBasicSipDialog::reply_error(req,400,"Bad request - bad From HF", "", logger);
    return -1;
  }

  ctx.from_aor = RegisterCache::canonicalize_aor(from_parser.uri_str());
  DBG("parsed AOR: '%s'",ctx.from_aor.c_str());

  if(ctx.from_aor.empty()) {
    AmBasicSipDialog::reply_error(req,400,"Bad request - bad From HF", "", logger);
    return -1;
  }
  ctx.aor_parsed = true;

  return 0;
}

int _RegisterCache::parseContacts(RegisterCacheCtx& ctx,
				  const AmSipRequest& req,
                                  msg_logger *logger)
{
  if(ctx.contacts_parsed)
    return 0;

  if ((RegisterDialog::parseContacts(req.contact, ctx.contacts) < 0) ||
      (ctx.contacts.size() == 0)) {
    AmBasicSipDialog::reply_error(req, 400, "Bad Request", 
				  "Warning: Malformed contact\r\n", logger);
    return -1;
  }
  ctx.contacts_parsed = true;
  return 0;
}

int _RegisterCache::parseExpires(RegisterCacheCtx& ctx,
				 const AmSipRequest& req,
                                 msg_logger *logger)
{
  if(ctx.expires_parsed)
    return 0;

  // move Expires as separate header to contact parameter
  string expires_str = getHeader(req.hdrs, "Expires");
  if (!expires_str.empty() && str2i(expires_str, ctx.requested_expires)) {
    AmBasicSipDialog::reply_error(req, 400, "Bad Request", 
				  "Warning: Malformed expires\r\n", logger);
    return true; // error reply sent
  }
  ctx.expires_parsed = true;
  return 0;
}

bool _RegisterCache::throttleRegister(RegisterCacheCtx& ctx,
				      const AmSipRequest& req,
                                      msg_logger *logger)
{
  if (req.method != SIP_METH_REGISTER) {
    ERROR("unsupported method '%s'\n", req.method.c_str());
    return false; // fwd
  }

  if (req.contact.empty() || (req.contact == "*")) {
    // binding query or unregister
    DBG("req.contact.empty() || (req.contact == \"*\")\n");
    return false; // fwd
  }

  if ((parseAoR(ctx,req, logger) < 0) ||
      (parseContacts(ctx,req, logger) < 0) ||
      (parseExpires(ctx,req, logger) < 0)) {
    DBG("could not parse AoR, Contact or Expires\n");
    return true; // error reply sent
  }

  unsigned int default_expires;
  if(ctx.requested_expires && (ctx.requested_expires > ctx.max_ua_expires))
    default_expires = ctx.max_ua_expires;
  else
    default_expires = ctx.requested_expires;

  vector<pair<string, long int> > alias_updates;
  for(vector<AmUriParser>::iterator contact_it = ctx.contacts.begin();
      contact_it != ctx.contacts.end(); contact_it++) {

    map<string, string>::iterator expires_it = 
      contact_it->params.find("expires");

    long int contact_expires=0;
    if(expires_it == contact_it->params.end()) {
      if(!default_expires){
	DBG("!default_expires");
	return false; // fwd
      }

      contact_expires = default_expires;
      contact_it->params["expires"] = long2str(contact_expires);
    }
    else {
      if(!str2long(expires_it->second,contact_expires)) {
	AmBasicSipDialog::reply_error(req, 400, "Bad Request",
				      "Warning: Malformed expires\r\n", logger);
	return true; // error reply sent
      }

      if(!contact_expires) {
	DBG("!contact_expires");
	return false; // fwd
      }

      if(contact_expires && ctx.max_ua_expires &&
	 (contact_expires > (long int)ctx.max_ua_expires)) {

	contact_expires = ctx.max_ua_expires;
	contact_it->params["expires"] = long2str(contact_expires);
      }
    }

    RegBinding reg_binding;
    const string& uri = contact_it->uri_str();

    if(!getAlias(ctx.from_aor,uri,req.remote_ip,reg_binding) ||
       !reg_binding.reg_expire) {
      DBG("!getAlias(%s,%s,...) || !reg_binding.reg_expire",
	  ctx.from_aor.c_str(),uri.c_str());
      return false; // fwd
    }

    struct timeval now;
    gettimeofday(&now,NULL);
    contact_expires += now.tv_sec;

    if(contact_expires + 4 /* 4 seconds buffer */ 
       >= reg_binding.reg_expire) {
      DBG("%li + 4 >= %li",contact_expires,reg_binding.reg_expire);
      return false; // fwd
    }
    
    AliasEntry alias_entry;
    if(!findAliasEntry(reg_binding.alias, alias_entry) ||
       (alias_entry.source_ip != req.remote_ip) ||
       (alias_entry.source_port != req.remote_port)) {
      DBG("no alias entry or IP/port mismatch");
      return false; // fwd
    }

    alias_updates.push_back(make_pair<string,long int>(reg_binding.alias,
						       contact_expires));
  }

  // reply 200 w/ contacts
  vector<AmUriParser>::iterator it = ctx.contacts.begin();
  vector<pair<string, long int> >::iterator alias_update_it = 
    alias_updates.begin();

  string contact_hdr = SIP_HDR_COLSP(SIP_HDR_CONTACT) + it->print();
  assert(alias_update_it != alias_updates.end());
  if(!updateAliasExpires(alias_update_it->first,
			 alias_update_it->second)) {
    // alias not found ???
    return false; // fwd
  }
  it++;
  alias_update_it++;

  for(;it != ctx.contacts.end(); it++, alias_update_it++) {

    contact_hdr += ", " + it->print();

    assert(alias_update_it != alias_updates.end());
    if(!updateAliasExpires(alias_update_it->first,
			   alias_update_it->second)) {
      // alias not found ???
      return false; // fwd
    }
  }
  contact_hdr += CRLF;

  // send 200 reply
  AmBasicSipDialog::reply_error(req, 200, "OK", contact_hdr, logger);
  return true;
}

bool _RegisterCache::saveSingleContact(RegisterCacheCtx& ctx,
				       const AmSipRequest& req,
                                       msg_logger *logger)
{
  if (req.method != SIP_METH_REGISTER) {
    ERROR("unsupported method '%s'\n", req.method.c_str());
    return false;
  }

  if(parseAoR(ctx,req, logger) < 0) {
    return true;
  }

  if (req.contact.empty()) {
    string contact_hdr;
    map<string,string> alias_map;
    if(getAorAliasMap(ctx.from_aor, alias_map) &&
       !alias_map.empty()) {

      struct timeval now;
      gettimeofday(&now,NULL);

      AliasEntry alias_entry;
      if(findAliasEntry(alias_map.begin()->first,alias_entry) &&
	 (now.tv_sec > alias_entry.ua_expire)) {

	unsigned int exp = now.tv_sec - alias_entry.ua_expire;
	contact_hdr = SIP_HDR_COLSP(SIP_HDR_CONTACT)
	  + alias_entry.contact_uri + ";expires=" 
	  + int2str(exp) + CRLF;
      }
    }
    
    AmBasicSipDialog::reply_error(req, 200, "OK", contact_hdr);
    return true;
  }

  bool star_contact=false;
  unsigned int contact_expires=0;
  AmUriParser* contact=NULL;
  if (req.contact == "*") {
    // unregister everything
    star_contact = true;

    if(parseExpires(ctx,req, logger) < 0) {
      return true;
    }

    if(ctx.requested_expires != 0) {
      AmBasicSipDialog::reply_error(req, 400, "Bad Request",
				    "Warning: Expires not equal 0\r\n");
      return true;
    }
  }
  else if ((parseContacts(ctx,req, logger) < 0) ||
	   (parseExpires(ctx,req, logger) < 0)) {
    return true; // error reply sent
  }
  else if (ctx.contacts.size() != 1) {
    AmBasicSipDialog::reply_error(req, 403, "Forbidden",
				  "Warning: only one contact allowed\r\n", logger);
    return true; // error reply sent
  }
  else {
    
    contact = &ctx.contacts[0];
    if(contact->params.find("expires") != contact->params.end()) {
      DBG("contact->params[\"expires\"] = '%s'",
	  contact->params["expires"].c_str());
      if(str2i(contact->params["expires"],contact_expires)) {
	AmBasicSipDialog::reply_error(req, 400, "Bad Request",
				      "Warning: Malformed expires\r\n", logger);
	return true; // error reply sent
      }
      DBG("contact_expires = %u",contact_expires);
    }
    else {
      contact_expires = ctx.requested_expires;
    }
  }

  if(!contact_expires) {
    // unregister AoR
    remove(ctx.from_aor);
    AmBasicSipDialog::reply_error(req, 200, "OK", "", logger);
    return true;
  }
  assert(contact);  

  // throttle contact_expires
  unsigned int reg_expires = contact_expires;
  if(reg_expires && (reg_expires < ctx.min_reg_expires))
    reg_expires = ctx.min_reg_expires;
  
  unsigned int ua_expires = contact_expires;
  if(ua_expires && ctx.max_ua_expires && 
     (ua_expires > ctx.max_ua_expires))
    ua_expires = ctx.max_ua_expires;

  struct timeval now;
  gettimeofday(&now,NULL);

  reg_expires += now.tv_sec;

  AliasEntry alias_update;
  alias_update.aor = ctx.from_aor;
  alias_update.contact_uri = contact->uri_str();
  alias_update.source_ip = req.remote_ip;
  alias_update.source_port = req.remote_port;
  alias_update.remote_ua = getHeader(req.hdrs,"User-Agent");
  alias_update.trsp = req.trsp;
  alias_update.local_if = req.local_if;
  alias_update.ua_expire = ua_expires + now.tv_sec;

  update(reg_expires,alias_update);

  contact->params["expires"] = int2str(ua_expires);
  string contact_hdr = SIP_HDR_COLSP(SIP_HDR_CONTACT)
    + contact->print() + CRLF;

  AmBasicSipDialog::reply_error(req, 200, "OK", contact_hdr);
  return true;
}
