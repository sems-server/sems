#ifndef _tr_blacklist_h_
#define _tr_blacklist_h_

#include "hash_table.h"
#include "singleton.h"

#include "ip_util.h"
#include "wheeltimer.h"

/**
 * Blacklist bucket: key type
 */
struct bl_addr: public sockaddr_storage
{
  bl_addr();
  bl_addr(const bl_addr&);
  bl_addr(const sockaddr_storage*);

  unsigned int hash();
};

struct bl_addr_less
{
  bool operator() (const bl_addr& l, const bl_addr& r) const;
};

struct bl_entry;

typedef ht_map_bucket<bl_addr,bl_entry,
		      ht_delete<bl_entry>,
		      bl_addr_less> bl_bucket_base;

class blacklist_bucket
  : public bl_bucket_base
{
protected:
  bool insert(const bl_addr& k, bl_entry* v) {
    return bl_bucket_base::insert(k,v);
  }

public:
  blacklist_bucket(unsigned long id)
  : bl_bucket_base(id)
  {}

  bool insert(const bl_addr& addr, unsigned int duration /* ms */,
	      const char* reason);
  bool remove(const bl_addr& addr);
};

typedef blacklist_bucket::value_map::iterator blacklist_elmt;

struct bl_timer
  : public timer
{
  bl_addr addr;

  bl_timer()
    : timer(), addr()
  {}

  bl_timer(const bl_addr& addr, unsigned int expires)
    : timer(expires), addr(addr)
  {}

  void fire();
};

/**
 * Blacklist bucket: value type
 */
struct bl_entry
{
  bl_timer* t;

  bl_entry() {}

  bl_entry(bl_timer* t)
    : t(t)
  {}
};

typedef hash_table<blacklist_bucket> blacklist_ht;

class _tr_blacklist
  : protected blacklist_ht
{
protected:
  _tr_blacklist();
  ~_tr_blacklist();

public:
  // public blacklist API:
  bool exist(const sockaddr_storage* addr);
  void insert(const sockaddr_storage* addr, unsigned int duration /* ms */,
	      const char* reason);
  void remove(const sockaddr_storage* addr);
};

typedef singleton<_tr_blacklist> tr_blacklist;

#endif
