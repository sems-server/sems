#include "fct.h"

#include "log.h"

#include "sip/tr_blacklist.h"

#include <arpa/inet.h>
#include <map>
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

} FCTMF_SUITE_END();
