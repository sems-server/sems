#ifndef _RegisterCache_h_
#define _RegisterCache_h_

#include "singleton.h"
#include "hash_table.h"

#include <string>
#include <map>
#include <memory>
using std::string;
using std::map;
using std::auto_ptr;

#define REG_CACHE_TABLE_POWER   10
#define REG_CACHE_TABLE_ENTRIES (1<<REG_CACHE_TABLE_POWER)

/*
 * Register cache:
 * ---------------
 * Data model:
 *  - canonical AoR <--1-to-n--> contacts
 *  - alias         <--1-to-1--> contact
 */

struct RegBinding
{
  // absolute timestamp
  long int expire;

  // unique-id used as contact user toward the registrar
  string alias;
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
};

struct RegCacheStorageHandler 
{
  virtual void onDelete(const string& aor, const string& uri, 
			const string& alias) {}

  virtual void onUpdate(const string& canon_aor, const string& alias, 
			long int expires, const AliasEntry& alias_update) {}
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
  string getAlias(const string& aor, const string& uri);

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
	      long int expires, const AliasEntry& alias_update);

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
  map<string,string> getAorAliasMap(const string& aor);

  /**
   * Retrieve the alias entry related to the given alias
   */
  bool findAliasEntry(const string& alias, AliasEntry& alias_entry);
};

typedef singleton<_RegisterCache> RegisterCache;

#endif
