#include "RegisterCache.h"
#include "sip/hash.h"
#include "sip/parse_uri.h"

#define REG_CACHE_CYCLE 10L /* 10 seconds to expire all buckets */

 /* in us */
#define REG_CACHE_SINGLE_CYCLE \
  ((REG_CACHE_CYCLE*1000000L)/REG_CACHE_TABLE_ENTRIES)

static unsigned int hash_1str(const string& aor)
{
  unsigned int h=0;
  h = hashlittle(aor.c_str(),aor.length(),h);
  return h & (REG_CACHE_TABLE_ENTRIES-1);
}

static string unescape_sip(const string& str)
{
  // TODO
  return str;
}

AorEntry* ContactCacheBucket::get(const string& aor)
{
  value_map::iterator it = find(aor);
  if(it == elmts.end())
    return NULL;
  
  return it->second;
}

void ContactCacheBucket::dump_elmt(const string& aor, 
				   const AorEntry* p_aor_entry) const
{
  DBG("'%s' ->", aor.c_str());
  if(!p_aor_entry) return;

  for(AorEntry::const_iterator it = p_aor_entry->begin();
      it != p_aor_entry->end(); it++) {

    if(it->second) {
      const RegBinding* b = it->second;
      DBG("\t'%s'", b ? b->alias.c_str() : "NULL");
    }
  }
}

void ContactCacheBucket::gbc(long int now, list<string>& alias_list)
{
  for(value_map::iterator it = elmts.begin(); it != elmts.end();) {

    AorEntry* aor_e = it->second;
    if(aor_e) {

      for(AorEntry::iterator reg_it = aor_e->begin();
	  reg_it != aor_e->end();) {

	RegBinding* binding = reg_it->second;

	if(binding && (binding->expire <= now)) {

	  alias_list.push_back(binding->alias);
	  AorEntry::iterator del_it = reg_it++;

	  DBG("delete binding: '%s' -> '%s' (%li <= %li)",
	      del_it->first.c_str(),binding->alias.c_str(),
	      binding->expire,now);

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

void AliasBucket::dump_elmt(const string& alias, const string* p_uri) const
{
  DBG("'%s' -> '%s'", alias.c_str(), 
      p_uri ? p_uri->c_str() : "NULL");
}

_RegisterCache::_RegisterCache()
  : reg_cache_ht(REG_CACHE_TABLE_ENTRIES),
    id_idx(REG_CACHE_TABLE_ENTRIES)
{
}

_RegisterCache::~_RegisterCache()
{
  DBG("##### REG CACHE DUMP #####");
  reg_cache_ht.dump();
  DBG("##### ID IDX DUMP #####");
  id_idx.dump();
  DBG("##### DUMP END #####");
}

void _RegisterCache::gbc(unsigned int bucket_id)
{
  // if(!bucket_id) {
  //   DBG("REG CACHE GBC CYCLE starting...");
  // }

  struct timeval now;
  gettimeofday(&now,NULL);

  ContactCacheBucket* bucket = reg_cache_ht.get_bucket(bucket_id);
  bucket->lock();
  list<string> alias_list;
  bucket->gbc(now.tv_sec,alias_list);
  for(list<string>::iterator it = alias_list.begin();
      it != alias_list.end(); it++){
    AliasBucket* alias_bucket = getAliasBucket(*it);
    alias_bucket->lock();
    alias_bucket->remove(*it);
    alias_bucket->unlock();
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
    nanosleep(&tick,&rem);
    gbc(gbc_bucket_id);
    gbc_bucket_id = (gbc_bucket_id+1);
    gbc_bucket_id &= (REG_CACHE_TABLE_ENTRIES-1);
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

ContactCacheBucket* _RegisterCache::getContactBucket(const string& aor)
{
  return reg_cache_ht.get_bucket(hash_1str(aor));
}

string _RegisterCache::getAlias(const string& canon_aor, const string& uri)
{
  string alias;
  //string canon_aor = canonicalize_aor(aor);
  if(canon_aor.empty()) {
    DBG("Canonical AOR is empty");
    return "";
  }

  ContactCacheBucket* bucket = getContactBucket(canon_aor);
  bucket->lock();

  AorEntry* aor_e = bucket->get(canon_aor);
  if(aor_e){
    AorEntry::iterator binding_it = aor_e->find(uri);
    if((binding_it != aor_e->end()) && binding_it->second)
      alias = binding_it->second->alias;
  }

  bucket->unlock();

  return alias;
}

AliasBucket* _RegisterCache::getAliasBucket(const string& alias)
{
  return id_idx.get_bucket(hash_1str(alias));
}

void _RegisterCache::update(const string& canon_aor, const string& alias, 
			    long int expires, const AliasEntry& alias_update)
{
  string uri = alias_update.contact_uri;
  //string canon_aor = canonicalize_aor(aor);
  if(canon_aor.empty()) {
    DBG("Canonical AOR is empty");
    return;
  }

  ContactCacheBucket* bucket = getContactBucket(canon_aor);
  AliasBucket* alias_bucket = getAliasBucket(alias);

  bucket->lock();
  alias_bucket->lock();

  // Try to get the existing binding
  RegBinding* binding = NULL;
  AorEntry* aor_e = bucket->get(canon_aor);
  if(!aor_e){
    // insert AorEntry if none
    aor_e = new AorEntry();
    bucket->insert(canon_aor,aor_e);
    DBG("inserted new AOR '%s'",canon_aor.c_str());
  }
  else {
    AorEntry::iterator binding_it = aor_e->find(uri);
    if(binding_it != aor_e->end()) {
      binding = binding_it->second;
    }
  }
  
  if(!binding) {
    // insert one if none exist
    binding = new RegBinding();
    binding->alias = alias;
    aor_e->insert(AorEntry::value_type(uri,binding));
    DBG("inserted new binding: '%s' -> '%s'",
	uri.c_str(), alias.c_str());
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
  binding->expire = expires;

  AliasEntry* alias_e = alias_bucket->getContact(alias);
  // if no alias map entry, insert a new one
  if(!alias_e) {
    DBG("inserting alias map entry: '%s' -> '%s'",
	alias.c_str(), uri.c_str());
    alias_bucket->insert(alias,new AliasEntry(alias_update));
  }
  else {
    *alias_e = alias_update;
  }
  
  alias_bucket->unlock();
  bucket->unlock();
}

void _RegisterCache::remove(const string& canon_aor, const string& uri, 
			    const string& alias)
{
  //string canon_aor = canonicalize_aor(aor);
  if(canon_aor.empty()) {
    DBG("Canonical AOR is empty");
    return;
  }

  ContactCacheBucket* bucket = getContactBucket(canon_aor);
  AliasBucket* alias_bucket = getAliasBucket(alias);
  
  bucket->lock();
  alias_bucket->lock();

  DBG("removing entries for aor = '%s', uri = '%s' and alias = '%s'",
      canon_aor.c_str(), uri.c_str(), alias.c_str());

  AorEntry* aor_e = bucket->get(canon_aor);
  if(aor_e) {
    AorEntry::iterator binding_it = aor_e->find(uri);
    if(binding_it != aor_e->end()) {
      delete binding_it->second;
      aor_e->erase(binding_it);
    }
    if(aor_e->empty()) {
      bucket->remove(canon_aor);
    }
  }
  alias_bucket->remove(alias);
  
  alias_bucket->unlock();
  bucket->unlock();
}

map<string,string> _RegisterCache::getAorAliasMap(const string& canon_aor)
{
  map<string,string> alias_map;

  if(canon_aor.empty()) {
    DBG("Canonical AOR is empty");
    return alias_map;
  }

  ContactCacheBucket* bucket = getContactBucket(canon_aor);
  bucket->lock();
  AorEntry* aor_e = bucket->get(canon_aor);
  if(aor_e) {
    for(AorEntry::iterator it = aor_e->begin();
	it != aor_e->end(); ++it) {

      if(it->second) {
	alias_map[it->second->alias] = it->first;
      }
    }
  }
  bucket->unlock();

  return alias_map;
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
