#include "fct.h"

#include "log.h"

#include "sip/ip_util.h"
#include "sip/tcp_trsp.h"

#include <event2/event.h>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// tcp_server_worker keeps one "peer_ip:peer_port" alias per peer. A peer that
// reconnects from a fixed source port (5060 is typical for proxies and
// trunking gear) yields a second connection with the very same alias, which
// add_connection() rebinds to the new socket. The connection it displaced is
// still open, and must be torn down by its own close() later on without
// touching the alias that now belongs to the newer connection.
//
// The connections here are socketpairs handed to create_connected() with a
// shared fake peer address, all driven on an event base owned by the test.

// macOS has no MSG_NOSIGNAL; SO_NOSIGPIPE is set on the socket there instead
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

namespace {

bool fd_is_open(int fd) { return fcntl(fd, F_GETFD) != -1 || errno != EBADF; }

// SOCK_NONBLOCK is not available everywhere (macOS)
bool nonblocking_socketpair(int sv[2]) {
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
    return false;
  }
  for (int i = 0; i < 2; i++) {
    int flags = fcntl(sv[i], F_GETFL);
    if (flags < 0 || fcntl(sv[i], F_SETFL, flags | O_NONBLOCK) < 0) {
      return false;
    }
  }
  return true;
}

void pump(struct event_base *evbase) {
  for (int i = 0; i < 10; i++) {
    event_base_loop(evbase, EVLOOP_NONBLOCK);
  }
}

struct alias_fixture {
  struct event_base *evbase;
  tcp_server_socket server;
  tcp_server_worker worker;
  sockaddr_storage peer;
  int refused_sd; // reserves the fake peer's port; bound, never listening
  int old_conn[2];
  int new_conn[2];

  alias_fixture() : evbase(event_base_new()), server(0), worker(&server), refused_sd(-1) {
    server.set_connect_timeout(0);
    server.set_idle_timeout(0);

    // should the alias get lost, worker.send() connects to the peer address:
    // make that fail at once instead of reaching anything real
    memset(&peer, 0, sizeof(peer));
    sockaddr_in *sin = (sockaddr_in *)&peer;
    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    refused_sd = socket(AF_INET, SOCK_STREAM, 0);
    socklen_t len = sizeof(peer);
    if (refused_sd < 0 || bind(refused_sd, (sockaddr *)&peer, sizeof(*sin)) ||
        getsockname(refused_sd, (sockaddr *)&peer, &len)) {
      ERROR("could not reserve a TCP port: %s", strerror(errno));
    }

    old_conn[0] = old_conn[1] = new_conn[0] = new_conn[1] = -1;
    if (!nonblocking_socketpair(old_conn) || !nonblocking_socketpair(new_conn)) {
      ERROR("could not create the test connections: %s", strerror(errno));
    }

    // same peer address twice: the second connection takes over the alias
    tcp_trsp_socket::create_connected(&server, &worker, old_conn[0], &peer, evbase);
    tcp_trsp_socket::create_connected(&server, &worker, new_conn[0], &peer, evbase);
  }

  ~alias_fixture() {
    // let the newer connection go away the regular way, too
    if (new_conn[1] >= 0) {
      close(new_conn[1]);
    }
    pump(evbase);

    // only on a regression: whatever the transport failed to close
    for (int fd : {old_conn[0], new_conn[0]}) {
      if (fd >= 0 && fd_is_open(fd)) {
        close(fd);
      }
    }
    if (refused_sd >= 0) {
      close(refused_sd);
    }
    event_base_free(evbase);
  }

  // the peer drops the connection that was displaced
  void close_old_conn() {
    close(old_conn[1]);
    old_conn[1] = -1;
    pump(evbase);
  }
};

// One connection from a fake peer, for feeding the transport raw bytes the
// way a peer would.
struct conn_fixture {
  struct event_base *evbase;
  tcp_server_socket server;
  tcp_server_worker worker;
  sockaddr_storage peer;
  int refused_sd; // reserves the fake peer's port; bound, never listening
  int conn[2];

  conn_fixture() : evbase(event_base_new()), server(0), worker(&server), refused_sd(-1) {
    server.set_connect_timeout(0);
    server.set_idle_timeout(0);

    memset(&peer, 0, sizeof(peer));
    sockaddr_in *sin = (sockaddr_in *)&peer;
    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    refused_sd = socket(AF_INET, SOCK_STREAM, 0);
    socklen_t len = sizeof(peer);
    if (refused_sd < 0 || bind(refused_sd, (sockaddr *)&peer, sizeof(*sin)) ||
        getsockname(refused_sd, (sockaddr *)&peer, &len)) {
      ERROR("could not reserve a TCP port: %s", strerror(errno));
    }

    conn[0] = conn[1] = -1;
    if (!nonblocking_socketpair(conn)) {
      ERROR("could not create the test connection: %s", strerror(errno));
    }
#ifdef SO_NOSIGPIPE
    int on = 1;
    setsockopt(conn[1], SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#endif
    tcp_trsp_socket::create_connected(&server, &worker, conn[0], &peer, evbase);
  }

  ~conn_fixture() {
    if (conn[1] >= 0) {
      close(conn[1]);
    }
    pump(evbase);
    if (conn[0] >= 0 && fd_is_open(conn[0])) {
      close(conn[0]);
    }
    if (refused_sd >= 0) {
      close(refused_sd);
    }
    event_base_free(evbase);
  }

  // the peer sends raw bytes on the connection; should the transport have
  // closed it, that fails the check instead of raising SIGPIPE
  bool peer_sends(const char *data) {
    size_t len = strlen(data);
    bool ok = send(conn[1], data, len, MSG_NOSIGNAL) == (ssize_t)len;
    pump(evbase);
    return ok;
  }

  // the transport can still reach the peer over this very connection
  bool still_usable() {
    const char msg[] = "OPTIONS sip:x SIP/2.0\r\n\r\n";
    if (worker.send(&peer, msg, sizeof(msg) - 1, 0) != 0) {
      return false;
    }
    pump(evbase);
    char buf[128] = {0};
    int got = read(conn[1], buf, sizeof(buf) - 1);
    return got == (int)sizeof(msg) - 1 && !strcmp(buf, msg);
  }
};

// a provisional reply that matches no transaction: the transaction layer
// parses it and drops it, nothing else gets involved
#define UNMATCHED_180(extra_hdrs)                                              \
  "SIP/2.0 180 Ringing\r\n"                                                    \
  "Via: SIP/2.0/TCP 127.0.0.1:5060;branch=z9hG4bK-tcp-test\r\n"                \
  "From: <sip:a@example.com>;tag=1\r\n"                                        \
  "To: <sip:b@example.com>;tag=2\r\n"                                          \
  "Call-ID: tcp-empty-hdr-test\r\n"                                            \
  "CSeq: 1 INVITE\r\n" extra_hdrs

} // namespace

FCTMF_SUITE_BGN(test_tcp_trsp) {

  // add_connection() used to release the map's reference to the connection
  // it displaced. That was the only reference the connection had, so it was
  // destroyed right there - without ever closing its socket.
  FCT_TEST_BGN(tcp_displaced_connection_is_closed_not_leaked) {
    alias_fixture f;
    fct_chk(fd_is_open(f.old_conn[0]));

    f.close_old_conn();
    fct_chk(!fd_is_open(f.old_conn[0]));
  }
  FCT_TEST_END();

  // Once the displaced connection closed, remove_connection() looked the
  // alias up by address alone and released the newer connection: that one
  // was destroyed with its socket left open, and send() had no connection
  // to the peer any more.
  FCT_TEST_BGN(tcp_closing_displaced_connection_keeps_alias) {
    alias_fixture f;
    f.close_old_conn();

    const char msg[] = "OPTIONS sip:x SIP/2.0\r\n\r\n";
    // fct_chk_eq_*() evaluate their arguments more than once
    int sent = f.worker.send(&f.peer, msg, sizeof(msg) - 1, 0);
    fct_chk_eq_int(sent, 0);
    pump(f.evbase);

    char buf[128] = {0};
    int got = read(f.new_conn[1], buf, sizeof(buf) - 1);
    fct_chk_eq_int(got, (int)sizeof(msg) - 1);
    fct_chk_eq_str(buf, msg);

    // and the newer connection still gets closed when its peer leaves
    fct_chk(fd_is_open(f.new_conn[0]));
    close(f.new_conn[1]);
    f.new_conn[1] = -1;
    pump(f.evbase);
    fct_chk(!fd_is_open(f.new_conn[0]));
  }
  FCT_TEST_END();

  // An empty header value is legal (RFC 3261 7.3.1), and the stream framer
  // used to call it malformed - which makes parse_input() drop the connection
  // with every transaction and dialog multiplexed on it. Any peer, or a proxy
  // relaying someone else's message, could do that with a valid message.
  FCT_TEST_BGN(tcp_empty_header_value_keeps_connection) {
    conn_fixture f;
    fct_req(f.peer_sends(UNMATCHED_180("Subject:\r\n"
                                       "Content-Length: 0\r\n"
                                       "\r\n")));
    fct_chk(fd_is_open(f.conn[0]));

    // the framing is intact: a message right after it is read as such
    fct_req(f.peer_sends(UNMATCHED_180("Content-Length: 0\r\n"
                                       "\r\n")));
    fct_chk(fd_is_open(f.conn[0]));
    fct_chk(f.still_usable());
  }
  FCT_TEST_END();

  // Content-Length is the exception: an empty one cannot be framed, and
  // treating it as 0 would read the body as the start of the next message.
  FCT_TEST_BGN(tcp_empty_content_length_closes_connection) {
    conn_fixture f;
    fct_req(f.peer_sends(UNMATCHED_180("Content-Length:\r\n"
                                       "\r\n"
                                       "INVITE sip:smuggled SIP/2.0\r\n\r\n")));
    fct_chk(!fd_is_open(f.conn[0]));
  }
  FCT_TEST_END();
}
FCTMF_SUITE_END();
