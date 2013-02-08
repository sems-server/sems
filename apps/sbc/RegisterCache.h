#ifndef _RegisterCache_h_
#define _RegisterCache_h_

#include "singleton.h"
#include "hash_table.h"

#include "AmSipMsg.h"
#include "AmUriParser.h"

#include <string>
#include <map>
#include <memory>
using std::string;
using std::map;
using std::auto_ptr;

#define REG_CACHE_TABLE_POWER   10
#define REG_CACHE_TABLE_ENTRIES (1<<REG_CACHE_TABLE_POWER)

#define DEFAULT_REG_EXPIRES 3600

/*
 * Register cache:
 * ---------------
 * Data model:
 *  - canonical AoR <--1-to-n--> contacts
 *  - alias         <--1-to-1--> contact
 */

struct RegBinding
{
  // Absolute timestamp representing
  // the expiration timer at the 
  // registrar side
  long int reg_expire;

  // unique-id used as contact user toward the registrar
  string alias;

  RegBinding()
    : reg_expire(0)
  {}
};

// Contact-URI -> RegBinding
typedef map<string,RegBinding*> AorEntry;

struct AliasEntry
{
  string contact_uri;

  // saved state for NAT handling
  string         source_ip;
  unsigned short source_port;

  // sticky interface
  unsigned short local_if;

  // Absolute timestamp representing
  // the expiration timer at the 
  // registered UA side
  long int ua_expire;

  AliasEntry()
    : source_port(0), local_if(0), ua_expire(0)
  {}
};

struct RegCacheStorageHandler 
{
  virtual void onDelete(const string& aor, const string& uri, 
			const string& alias) {}

  virtual void onUpdate(const string& canon_aor, const string& alias, 
			long int expires, const AliasEntry& alias_update) {}

  virtual void onUpdate(const string& alias, long int ua_expires) {}
};

/**
 * Hash-table bucket:
 *   AoR -> AorEntry
 */
class ContactCacheBucket
  : public ht_map_bucket<string,AorEntry>
{
public:
  ContactCacheBucket(unsigned long id)
  : ht_map_bucket<string,AorEntry>(id) 
  {}

  /**
   * Match and retrieve the cache entry associated with the AOR passed.
   * aor: canonicalized AOR
   */
  AorEntry* get(const string& aor);

  /* Maintenance stuff */

  void gbc(RegCacheStorageHandler* h, long int now, list<string>& alias_list);
  void dump_elmt(const string& aor, const AorEntry* p_aor_entry) const;
};

/**
 * Hash-table bucket:
 *   Alias -> Contact-URI
 */
class AliasBucket
  : public ht_map_bucket<string,AliasEntry>
{
public:
  AliasBucket(unsigned long int id)
  : ht_map_bucket<string,AliasEntry>(id)
  {}

  AliasEntry* getContact(const string& alias);

  void dump_elmt(const string& alias, const string* p_uri) const;
};

/** 
 * Registrar/Reg-Caching 
 * parsing/processing context 
 */
struct RegisterCacheCtx
  : public AmObject
{
  string              from_aor;
  bool               aor_parsed;

  vector<AmUriParser> contacts;
  bool         contacts_parsed;

  unsigned int requested_expires;
  bool            expires_parsed;

  unsigned int min_reg_expires;
  unsigned int max_ua_expires;

  RegisterCacheCtx()
    : aor_parsed(false),
      contacts_parsed(false),
      requested_expires(DEFAULT_REG_EXPIRES),
      expires_parsed(false),
      min_reg_expires(0),
      max_ua_expires(0)
  {}
};

class _RegisterCache
  : public AmThread
{
  hash_table<ContactCacheBucket> reg_cache_ht;
  hash_table<AliasBucket>        id_idx;

  auto_ptr<RegCacheStorageHandler> storage_handler;

  unsigned int gbc_bucket_id;

  AmSharedVar<bool> running;

  void gbc(unsigned int bucket_id);

protected:
  _RegisterCache();
  ~_RegisterCache();

  void dispose() { stop(); }

  /* AmThread interface */
  void run();
  void on_stop();

  /**
   * Returns the bucket associated with the passed contact-uri
   * aor: canonicalized AOR
   */
  ContactCacheBucket* getContactBucket(const string& aor);

  /**
   * Returns the bucket associated with the alias given
   * alias: Contact-user
   */
  AliasBucket* getAliasBucket(const string& alias);

  int parseAoR(RegisterCacheCtx& ctx, const AmSipRequest& req);
  int parseContacts(RegisterCacheCtx& ctx, const AmSipRequest& req);
  int parseExpires(RegisterCacheCtx& ctx, const AmSipRequest& req);

public:
  static string canonicalize_aor(const string& aor);

  void setStorageHandler(RegCacheStorageHandler* h) { storage_handler.reset(h); }

  /**
   * Match, retrieve the contact cache entry associated with the URI passed,
   * and return the alias found in the cache entry.
   *
   * Note: this function locks and unlocks the contact cache bucket.
   *
   * aor: canonical Address-of-Record
   * uri: Contact-URI
   */
  bool getAlias(const string& aor, const string& uri,
		RegBinding& out_binding);

  /**
   * Update contact cache entry and alias map entries.
   *
   * Note: this function locks and unlocks 
   *       the contact cache bucket and
   *       the alias map bucket.
   *
   * aor: canonical Address-of-Record
   * uri: Contact-URI
   * alias: 
   */
  void update(const string& canon_aor, const string& alias, 
	      long int reg_expires, const AliasEntry& alias_update);

  void update(const string& canon_aor, long int reg_expires,
	      const AliasEntry& alias_update);

  void updateAliasExpires(const string& alias, long int ua_expires);

  /**
   * Remove contact cache entry and alias map entries.
   *
   * Note: this function locks and unlocks 
   *       the contact cache bucket and
   *       the alias map bucket.
   *
   * aor: canonical Address-of-Record
   * uri: Contact-URI
   * alias:
   */
  void remove(const string& aor, const string& uri, const string& alias);

  void remove(const string& aor);

  /**
   * Retrieve an alias map containing all entries related
   * to a particular AOR. This is needed to support REGISTER
   * with '*' contact.
   *
   * Note: this function locks and unlocks 
   *       the contact cache bucket.
   *
   * aor: canonical Address-of-Record
   */
  bool getAorAliasMap(const string& aor, map<string,string>& alias_map);

  /**
   * Retrieve the alias entry related to the given alias
   */
  bool findAliasEntry(const string& alias, AliasEntry& alias_entry);

  /**
   * Throttle REGISTER requests
   *
   * Returns false if REGISTER should be forwarded:
   * - if registrar binding should be renewed.
   * - if source IP or port do not match the saved IP & port.
   * - if the request unregisters any contact.
   * - if request is not a REGISTER
   */
  bool throttleRegister(RegisterCacheCtx& ctx,
			const AmSipRequest& req);

  /**
   * Throttle REGISTER requests
   *
   * Returns false if failed:
   * - if request is not a REGISTER.
   * - more than one contact should be (un)registered.
   */
  bool saveSingleContact(RegisterCacheCtx& ctx,
			const AmSipRequest& req);
};

typedef singleton<_RegisterCache> RegisterCache;

#endif
