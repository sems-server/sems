#include "fct.h"

#include "log.h"
#include "AmUtils.h"

#include "sip/parse_next_hop.h"
#include "sip/resolver.h"
#include "sip/ip_util.h"

#include <list>
#include <string>
#include <vector>

// End-to-end tests of _resolver against a DNS server running inside the test
// process, driven through resolve_targets() the way the transaction layer
// resolves a next hop.
//
// The server is reached through the thread-local resolver state of the libc
// (_res), which is only known to work this way with glibc, so the tests are
// compiled there only.

#if defined(__GLIBC__)

#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <resolv.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>

namespace {

struct FakeRR {
  std::string name;
  unsigned short type;
  std::string rdata;
};

std::string u16(unsigned short v) {
  std::string s;
  s += (char)(v >> 8);
  s += (char)(v & 0xff);
  return s;
}

std::string u32(unsigned int v) { return u16(v >> 16) + u16(v & 0xffff); }

std::string encode_name(const std::string &name) {
  std::string res;
  size_t b = 0;
  while (b < name.size()) {
    size_t e = name.find('.', b);
    if (e == std::string::npos) {
      e = name.size();
    }
    res += (char)(e - b);
    res += name.substr(b, e - b);
    b = e + 1;
  }
  return res + '\0';
}

FakeRR a_rr(const std::string &name, const char *ip) {
  in_addr a;
  inet_pton(AF_INET, ip, &a);
  return FakeRR{name, ns_t_a, std::string((const char *)&a, sizeof(a))};
}

FakeRR srv_rr(const std::string &name, unsigned short prio, unsigned short weight, unsigned short port,
              const std::string &target) {
  return FakeRR{name, ns_t_srv, u16(prio) + u16(weight) + u16(port) + encode_name(target)};
}

/**
 * Minimal authoritative DNS server on 127.0.0.1 (UDP and TCP on the same
 * port). Answers carry the records of the zone matching the question's name
 * and type; a name without any record is NXDOMAIN. As a real server does, an
 * answer that does not fit into 512 bytes is replaced by an empty one with TC
 * set over UDP, so the client retries over TCP and gets the whole answer.
 */
class FakeDnsServer {
public:
  std::vector<FakeRR> zone;

  FakeDnsServer() : udp_fd(-1), tcp_fd(-1), port(0), running(false), stop(false) {}

  ~FakeDnsServer() {
    if (running) {
      stop.store(true);
      pthread_join(tid, NULL);
    }
    if (udp_fd >= 0) {
      close(udp_fd);
    }
    if (tcp_fd >= 0) {
      close(tcp_fd);
    }
  }

  bool start() {
    sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    // find a port that is free for both UDP and TCP
    for (int attempt = 0; attempt < 20; attempt++) {
      udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
      sa.sin_port = 0;
      if (udp_fd < 0 || bind(udp_fd, (sockaddr *)&sa, sizeof(sa)) < 0) {
        return false;
      }

      socklen_t sa_len = sizeof(sa);
      getsockname(udp_fd, (sockaddr *)&sa, &sa_len);

      tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
      int on = 1;
      setsockopt(tcp_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
      if (bind(tcp_fd, (sockaddr *)&sa, sizeof(sa)) == 0 && listen(tcp_fd, 8) == 0) {
        port = ntohs(sa.sin_port);
        break;
      }

      close(udp_fd);
      close(tcp_fd);
      udp_fd = tcp_fd = -1;
    }

    if (!port) {
      return false;
    }

    running = (pthread_create(&tid, NULL, run_thread, this) == 0);
    return running;
  }

  /** point this thread's libc resolver at the server */
  void use() const {
    res_init();
    _res.nscount = 1;
    _res.nsaddr_list[0].sin_family = AF_INET;
    _res.nsaddr_list[0].sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    _res.nsaddr_list[0].sin_port = htons(port);
    _res.options &= ~(RES_DNSRCH | RES_DEFNAMES);
    _res.retrans = 1;
    _res.retry = 1;
  }

  /** go back to the system's resolver configuration */
  static void restore() { res_init(); }

private:
  int udp_fd;
  int tcp_fd;
  unsigned short port;
  pthread_t tid;
  bool running;
  std::atomic<bool> stop;

  static void *run_thread(void *arg) {
    static_cast<FakeDnsServer *>(arg)->run();
    return NULL;
  }

  void run() {
    while (!stop.load()) {
      pollfd fds[2] = {{udp_fd, POLLIN, 0}, {tcp_fd, POLLIN, 0}};
      if (poll(fds, 2, 50) <= 0) {
        continue;
      }

      if (fds[0].revents & POLLIN) {
        unsigned char q[NS_PACKETSZ];
        sockaddr_storage from;
        socklen_t from_len = sizeof(from);
        ssize_t len = recvfrom(udp_fd, q, sizeof(q), 0, (sockaddr *)&from, &from_len);
        if (len > 0) {
          std::string r = answer(q, len, true);
          sendto(udp_fd, r.data(), r.size(), 0, (sockaddr *)&from, from_len);
        }
      }

      if (fds[1].revents & POLLIN) {
        int c = accept(tcp_fd, NULL, NULL);
        if (c >= 0) {
          serve_tcp(c);
          close(c);
        }
      }
    }
  }

  static bool read_all(int fd, unsigned char *buf, size_t len) {
    while (len) {
      pollfd p = {fd, POLLIN, 0};
      if (poll(&p, 1, 2000) <= 0) {
        return false;
      }
      ssize_t n = read(fd, buf, len);
      if (n <= 0) {
        return false;
      }
      buf += n;
      len -= n;
    }
    return true;
  }

  void serve_tcp(int c) {
    unsigned char hdr[2];
    while (read_all(c, hdr, 2)) {
      size_t len = (hdr[0] << 8) | hdr[1];
      std::vector<unsigned char> q(len);
      if (!len || !read_all(c, &q[0], len)) {
        return;
      }
      std::string r = answer(&q[0], len, false);
      r = u16(r.size()) + r;
      if (write(c, r.data(), r.size()) != (ssize_t)r.size()) {
        return;
      }
    }
  }

  std::string answer(const unsigned char *q, size_t len, bool udp) {
    if (len < 12) {
      return std::string();
    }

    // question: name, type, class
    std::string qname;
    size_t p = 12;
    while (p < len && q[p]) {
      if (!qname.empty()) {
        qname += '.';
      }
      qname.append((const char *)q + p + 1, q[p]);
      p += q[p] + 1;
    }
    if (p + 5 > len) {
      return std::string();
    }
    unsigned short qtype = (q[p + 1] << 8) | q[p + 2];
    size_t question_end = p + 5;

    bool name_exists = false;
    std::string an;
    unsigned short an_count = 0;
    for (size_t i = 0; i < zone.size(); i++) {
      if (strcasecmp(zone[i].name.c_str(), qname.c_str())) {
        continue;
      }
      name_exists = true;
      if (zone[i].type != qtype) {
        continue;
      }
      an += u16(0xc00c); // the question's name
      an += u16(zone[i].type) + u16(ns_c_in) + u32(60);
      an += u16(zone[i].rdata.size()) + zone[i].rdata;
      an_count++;
    }

    std::string question((const char *)q + 12, question_end - 12);
    unsigned short flags = 0x8000 /* QR */ | 0x0400 /* AA */ | 0x0080 /* RA */ |
                           (q[2] & 0x01 ? 0x0100 : 0) /* RD */ | (name_exists ? 0 : ns_r_nxdomain);

    std::string head = std::string((const char *)q, 2) + u16(flags) + u16(1);
    std::string full = head + u16(an_count) + u16(0) + u16(0) + question + an;

    if (udp && full.size() > NS_PACKETSZ) {
      // does not fit: truncate, the client has to come back over TCP
      return head.substr(0, 2) + u16(flags | 0x0200 /* TC */) + u16(1) + u16(0) + u16(0) + u16(0) + question;
    }

    return full;
  }
};

std::string target_str(const sip_target &t) {
  return am_inet_ntop(&t.ss) + ":" + int2str(am_get_port(&t.ss));
}

/** resolve 'host' as a next hop without an explicit port, as trans_layer does */
std::vector<std::string> resolve(const char *host) {
  std::list<sip_destination> dest_list;
  sip_destination d;
  d.host = cstring(host);
  d.trsp = cstring("udp");
  dest_list.push_back(d);

  sip_target_set targets;
  std::vector<std::string> res;
  if (resolver::instance()->resolve_targets(dest_list, &targets) < 0) {
    return res;
  }

  for (std::list<sip_target>::iterator it = targets.dest_list.begin(); it != targets.dest_list.end(); ++it) {
    res.push_back(target_str(*it));
  }

  return res;
}

std::string join(const std::vector<std::string> &v) {
  std::string res;
  for (size_t i = 0; i < v.size(); i++) {
    res += (i ? " " : "") + v[i];
  }
  return res;
}

} // namespace

FCTMF_SUITE_BGN(test_resolver_dns) {

  // An SRV record whose target is a host name - the normal SRV deployment -
  // used to crash: dns_srv_entry::next_ip() resolves the target through its
  // own handle, which already has the SRV entry bound, so resolve_name()
  // took the "continue with this handle" shortcut back into
  // dns_srv_entry::next_ip() and dereferenced the still unbound A entry.
  FCT_TEST_BGN(srv_target_host_names_are_resolved) {
    FakeDnsServer dns;
    dns.zone.push_back(srv_rr("_sip._udp.srv1.test", 10, 0, 5070, "a.srv1.test"));
    dns.zone.push_back(srv_rr("_sip._udp.srv1.test", 20, 0, 5080, "b.srv1.test"));
    dns.zone.push_back(a_rr("a.srv1.test", "192.0.2.1"));
    dns.zone.push_back(a_rr("a.srv1.test", "192.0.2.2"));
    dns.zone.push_back(a_rr("b.srv1.test", "192.0.2.3"));
    fct_req(dns.start());
    dns.use();

    // every address of the first target, with its SRV port, then the next
    // SRV record's target
    std::string expected = "192.0.2.1:5070 192.0.2.2:5070 192.0.2.3:5080";
    std::string got = join(resolve("srv1.test"));
    fct_xchk(got == expected, "got '%s', expected '%s'", got.c_str(), expected.c_str());

    // and the same again out of the DNS cache
    got = join(resolve("srv1.test"));
    fct_xchk(got == expected, "cached: got '%s', expected '%s'", got.c_str(), expected.c_str());

    FakeDnsServer::restore();
  }
  FCT_TEST_END();

  // An SRV target mixing an address literal with a host name: the literal
  // ends its "IP list" right away, the host name has to be looked up.
  FCT_TEST_BGN(srv_targets_may_mix_literals_and_host_names) {
    FakeDnsServer dns;
    dns.zone.push_back(srv_rr("_sip._udp.srv3.test", 10, 0, 5070, "192.0.2.30"));
    dns.zone.push_back(srv_rr("_sip._udp.srv3.test", 20, 0, 5080, "b.srv3.test"));
    dns.zone.push_back(a_rr("b.srv3.test", "192.0.2.31"));
    fct_req(dns.start());
    dns.use();

    std::string expected = "192.0.2.30:5070 192.0.2.31:5080";
    std::string got = join(resolve("srv3.test"));
    fct_xchk(got == expected, "got '%s', expected '%s'", got.c_str(), expected.c_str());

    FakeDnsServer::restore();
  }
  FCT_TEST_END();

  // When the SRV record exists but its target does not resolve,
  // set_destination_ip() falls back to the next hop's own A records. The
  // failed SRV attempt must not leave its entry and port on the handle: the
  // remaining fallback addresses used to be dispatched through the SRV entry
  // and stamped with the failed record's port.
  FCT_TEST_BGN(failed_srv_target_does_not_leak_into_fallback) {
    FakeDnsServer dns;
    dns.zone.push_back(srv_rr("_sip._udp.srv2.test", 10, 0, 5080, "nx.srv2.test"));
    dns.zone.push_back(a_rr("srv2.test", "192.0.2.10"));
    dns.zone.push_back(a_rr("srv2.test", "192.0.2.11"));
    fct_req(dns.start());
    dns.use();

    std::string expected = "192.0.2.10:5060 192.0.2.11:5060";
    std::string got = join(resolve("srv2.test"));
    fct_xchk(got == expected, "got '%s', expected '%s'", got.c_str(), expected.c_str());

    FakeDnsServer::restore();
  }
  FCT_TEST_END();

  // An answer that does not fit into a 512 byte UDP message is fetched over
  // TCP, and res_search() then reports the length of the whole answer even
  // where its buffer was too small to hold it. query_dns() used to parse that
  // many bytes out of a 512 byte stack buffer.
  FCT_TEST_BGN(answers_larger_than_a_udp_message_are_parsed_whole) {
    FakeDnsServer dns;
    std::vector<std::string> expected;

    // ~1.6kB of A records: more than 512 bytes, and more than 1024 as well
    for (int i = 1; i <= 100; i++) {
      std::string ip = "198.51.100." + int2str(i);
      dns.zone.push_back(a_rr("big.test", ip.c_str()));
      expected.push_back(ip + ":5060");
    }
    fct_req(dns.start());
    dns.use();

    std::vector<std::string> got = resolve("big.test");
    fct_xchk(got == expected, "got %u targets: '%s'", (unsigned int)got.size(), join(got).c_str());

    FakeDnsServer::restore();
  }
  FCT_TEST_END();
}
FCTMF_SUITE_END();

#else // !__GLIBC__

FCTMF_SUITE_BGN(test_resolver_dns) {}
FCTMF_SUITE_END();

#endif
