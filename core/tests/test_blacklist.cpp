#include "fct.h"

#include "log.h"

#include "sip/tr_blacklist.h"

#include <arpa/inet.h>
#include <map>
#include <stdio.h>
#include <string.h>

// bl_addr_less is the comparator of the std::maps that hold the transport
// blacklist buckets. It used to return the raw memcmp() result as a bool,
// which is "true" for any two different addresses in both directions and so
// is not a strict weak ordering: entries could no longer be found, removed
// or de-duplicated once more than one address was blacklisted.

static bl_addr bl_addr_v4(const char* ip, unsigned short port)
{
  sockaddr_storage ss;
  memset(&ss, 0, sizeof(ss));

  sockaddr_in* sin = (sockaddr_in*)&ss;
  sin->sin_family = AF_INET;
  sin->sin_port = htons(port);
  inet_pton(AF_INET, ip, &sin->sin_addr);

  return bl_addr(&ss);
}

static bl_addr bl_addr_v6(const char* ip, unsigned short port)
{
  sockaddr_storage ss;
  memset(&ss, 0, sizeof(ss));

  sockaddr_in6* sin6 = (sockaddr_in6*)&ss;
  sin6->sin6_family = AF_INET6;
  sin6->sin6_port = htons(port);
  inet_pton(AF_INET6, ip, &sin6->sin6_addr);

  return bl_addr(&ss);
}

// The real bucket, with its element count exposed. insert() and remove() are
// the ones the blacklist itself runs, timers included.
struct counting_bucket : public blacklist_bucket
{
  counting_bucket() : blacklist_bucket(0) {}
  size_t count() { return elmts.size(); }
};

// Up to n IPv4 addresses that all hash into the same blacklist bucket, the
// way _tr_blacklist picks it. The table has 64 buckets, so a few blacklisted
// destinations already share one.
static int colliding_addrs(bl_addr* out, int n)
{
  unsigned int bucket = bl_addr_v4("10.1.0.1", 5060).hash();
  int found = 0;
  for (unsigned int i = 1; i < 65536 && found < n; i++) {
    char ip[32];
    snprintf(ip, sizeof(ip), "10.1.%u.%u", i >> 8, i & 0xff);
    bl_addr a = bl_addr_v4(ip, 5060);
    if (a.hash() == bucket)
      out[found++] = a;
  }
  return found;
}

FCTMF_SUITE_BGN(test_blacklist) {

  FCT_TEST_BGN(bl_addr_less_is_a_strict_weak_ordering) {
    bl_addr_less less;

    bl_addr a = bl_addr_v4("10.0.0.1", 5060);
    bl_addr b = bl_addr_v4("10.0.0.2", 5060);
    bl_addr a2 = bl_addr_v4("10.0.0.1", 5060);
    bl_addr v6 = bl_addr_v6("2001:db8::1", 5060);

    // irreflexive
    fct_chk(!less(a, a));
    fct_chk(!less(a, a2));
    fct_chk(!less(a2, a));

    // asymmetric: exactly one of the two orderings holds
    fct_chk(less(a, b) != less(b, a));
    fct_chk(less(a, v6) != less(v6, a));

    // different ports of the same address are distinct and ordered
    bl_addr a_other_port = bl_addr_v4("10.0.0.1", 5080);
    fct_chk(less(a, a_other_port) != less(a_other_port, a));
  }
  FCT_TEST_END();

  FCT_TEST_BGN(bl_addr_keyed_map_finds_erases_and_dedups) {
    // this is how the blacklist buckets are keyed
    std::map<bl_addr, int, bl_addr_less> m;

    bl_addr addrs[5] = {
      bl_addr_v4("10.0.0.1", 5060),
      bl_addr_v4("10.0.0.2", 5060),
      bl_addr_v4("10.0.0.3", 5060),
      bl_addr_v4("192.168.1.7", 5060),
      bl_addr_v6("2001:db8::1", 5060),
    };

    for (int i = 0; i < 5; i++)
      m[addrs[i]] = i;
    fct_chk_eq_int((int)m.size(), 5);

    // every blacklisted address must be found again, otherwise the blacklist
    // silently has no effect at all
    for (int i = 0; i < 5; i++) {
      std::map<bl_addr, int, bl_addr_less>::iterator it = m.find(addrs[i]);
      fct_chk(it != m.end());
      if (it != m.end())
        fct_chk_eq_int(it->second, i);
    }

    // re-inserting the same addresses must not add duplicate entries
    for (int i = 0; i < 5; i++)
      m[addrs[i]] = i;
    fct_chk_eq_int((int)m.size(), 5);

    // ... and the expiry timers must be able to remove them again
    // note: fct_chk_eq_int() evaluates its arguments more than once, so the
    // erase() has to happen outside of it
    for (int i = 0; i < 5; i++) {
      int erased = (int)m.erase(addrs[i]);
      fct_chk_eq_int(erased, 1);
    }
    fct_chk(m.empty());
  }
  FCT_TEST_END();

  // The same through the bucket the blacklist actually uses: a blacklisted
  // destination has to be found again, a second failure must not add another
  // entry (and another timer), and the expiry has to take it out - otherwise
  // the entries and their timers pile up for as long as the process runs.
  FCT_TEST_BGN(blacklist_bucket_finds_dedups_and_removes) {
    const int N = 6;
    bl_addr addrs[N];
    fct_req(colliding_addrs(addrs, N) == N);

    counting_bucket b;
    b.lock();

    for (int i = 0; i < N; i++)
      fct_chk(b.insert(addrs[i], 3600 * 1000, "test"));
    fct_chk_eq_int((int)b.count(), N);

    for (int i = 0; i < N; i++)
      fct_xchk(b.exist(addrs[i]), "blacklisted address #%d not found", i);

    // what _tr_blacklist::insert() does on every further failure
    for (int i = 0; i < N; i++) {
      if (!b.exist(addrs[i]))
        b.insert(addrs[i], 3600 * 1000, "test");
    }
    fct_chk_eq_int((int)b.count(), N);

    // what bl_timer::fire() does once the TTL is over
    for (int i = 0; i < N; i++)
      fct_xchk(b.remove(addrs[i]), "blacklisted address #%d not removed", i);
    fct_chk_eq_int((int)b.count(), 0);

    b.unlock();
  }
  FCT_TEST_END();

  // End to end through the singleton the transaction layer consults.
  FCT_TEST_BGN(tr_blacklist_colliding_addresses) {
    const int N = 6;
    bl_addr addrs[N];
    fct_req(colliding_addrs(addrs, N) == N);

    for (int i = 0; i < N; i++)
      tr_blacklist::instance()->insert(&addrs[i], 3600 * 1000, "test");

    for (int i = 0; i < N; i++)
      fct_xchk(tr_blacklist::instance()->exist(&addrs[i]),
               "blacklisted address #%d not found", i);

    for (int i = 0; i < N; i++)
      tr_blacklist::instance()->remove(&addrs[i]);

    for (int i = 0; i < N; i++)
      fct_xchk(!tr_blacklist::instance()->exist(&addrs[i]),
               "address #%d still blacklisted after its removal", i);
  }
  FCT_TEST_END();

} FCTMF_SUITE_END();
