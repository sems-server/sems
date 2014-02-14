#ifndef _RegisterCache_h_
#define _RegisterCache_h_

#include "singleton.h"
#include "hash_table.h"
#include "atomic_types.h"

#include "AmSipMsg.h"
#include "AmUriParser.h"
#include "AmAppTimer.h"

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

// Contact-URI/Public-IP -> RegBinding
typedef map<string,RegBinding*> AorEntry;

struct AliasEntry
  : public DirectAppTimer
{
  string aor;
  string contact_uri;
  string alias;

  // saved state for NAT handling
  string         source_ip;
  unsigned short source_port;
  string         trsp;

  // sticky interface
  unsigned short local_if;

  // User-Agent
  string remote_ua;

  // Absolute timestamp representing
  // the expiration timer at the 
  // registered UA side
  long int ua_expire;

  AliasEntry()
    : source_port(0), local_if(0), ua_expire(0)
  {}

  // from DirectAppTimer
  void fire();
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
class AorBucket
  : public ht_map_bucket<string,AorEntry>
{
public:
  AorBucket(unsigned long id)
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

  void dump_elmt(const string& alias, const AliasEntry* ae) const;
};

class ContactBucket
  : public ht_map_bucket<string,string>
{
  typedef ht_map_bucket<string,string> Bucket;

  bool insert(const string& k, string* v) {
    return Bucket::insert(k,v);
  }

  bool remove(const string& k) {
    return Bucket::remove(k);
  }

public:
  ContactBucket(unsigned long int id)
  : ht_map_bucket<string,string>(id)
  {}

  void insert(const string& contact_uri, const string& remote_ip,
	      unsigned short remote_port, const string& alias);

  string getAlias(const string& contact_uri, const string& remote_ip,
		  unsigned short remote_port);

  void remove(const string& contact_uri, const string& remote_ip,
	      unsigned short remote_port);

  void dump_elmt(const string& key, const string* alias) const;
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
  hash_table<AorBucket> reg_cache_ht;
  hash_table<AliasBucket>        id_idx;
  hash_table<ContactBucket>      contact_idx;

  auto_ptr<RegCacheStorageHandler> storage_handler;

  unsigned int gbc_bucket_id;

  AmSharedVar<bool> running;

  // stats
  atomic_int active_regs;

  void gbc(unsigned int bucket_id);
  void removeAlias(const string& alias, bool generate_event);

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
  AorBucket* getAorBucket(const string& aor);

  /**
   * Returns the bucket associated with the alias given
   * alias: Contact-user
   */
  AliasBucket* getAliasBucket(const string& alias);

  /**
   * Returns the bucket associated with the contact-URI given
   */
  ContactBucket* getContactBucket(const string& contact_uri,
				  const string& remote_ip,
				  unsigned short remote_port);

  int parseAoR(RegisterCacheCtx& ctx, const AmSipRequest& req, msg_logger *logger);
  int parseContacts(RegisterCacheCtx& ctx, const AmSipRequest& req, msg_logger *logger);
  int parseExpires(RegisterCacheCtx& ctx, const AmSipRequest& req, msg_logger *logger);

  void setAliasUATimer(AliasEntry* alias_e);
  void removeAliasUATimer(AliasEntry* alias_e);

public:
  static string canonicalize_aor(const string& aor);
  static string compute_alias_hash(const string& aor, const string& contact_uri,
				   const string& public_ip);

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
		const string& public_ip, RegBinding& out_binding);

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
  void update(const string& alias, long int reg_expires,
	      const AliasEntry& alias_update);

  void update(long int reg_expires, const AliasEntry& alias_update);

  bool updateAliasExpires(const string& alias, long int ua_expires);

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
  void remove(const string& aor, const string& uri,
	      const string& alias);

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
   * alias_map: alias -> contact
   */
  bool getAorAliasMap(const string& aor, map<string,string>& alias_map);

  /**
   * Retrieve the alias entry related to the given alias
   */
  bool findAliasEntry(const string& alias, AliasEntry& alias_entry);

  /**
   * Retrieve the alias entry related to the given contact-URI, remote-IP & port
   */
  bool findAEByContact(const string& contact_uri, const string& remote_ip,
		       unsigned short remote_port, AliasEntry& ae);

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
			const AmSipRequest& req,
                        msg_logger *logger = NULL);

  /**
   * Save a single REGISTER contact into cache
   *
   * Returns false if failed:
   * - if request is not a REGISTER.
   * - more than one contact should be (un)registered.
   *
   * If true has been returned, the request has already 
   * been replied with either an error or 200 (w/ contact).
   *
   * Note: this function also handles binding query.
   *       (REGISTER w/o contacts)
   */
  bool saveSingleContact(RegisterCacheCtx& ctx,
			const AmSipRequest& req,
                        msg_logger *logger = NULL);

  /**
   * Statistics
   */
  unsigned int getActiveRegs() { return active_regs.get(); }
};

typedef singleton<_RegisterCache> RegisterCache;

#endif
