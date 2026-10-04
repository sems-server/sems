#include "fct.h"

#include "log.h"

#include "sip/msg_logger.h"
#include "sip/parse_cseq.h"
#include "sip/sip_parser.h"
#include "sip/sip_trans.h"
#include "sip/trans_layer.h"
#include "sip/trans_table.h"
#include "sip/transport.h"

#include <ctype.h>
#include <string.h>
#include <string>

// _trans_layer::send_request() serialises every outgoing request into a
// second sip_msg, which takes a reference on the local socket, and has to
// hand that message to an owner: a non-ACK request is kept by the
// transaction it creates, a 2xx-ACK over an unreliable transport gives its
// buffer to the INVITE transaction for retransmissions, and a 2xx-ACK over a
// reliable transport is never retransmitted, so it is kept by nobody and
// send_request() has to free it itself.
//
// The stub socket below stands in for the TCP/UDP transport. Once every
// sip_msg and transaction referring to it is gone, the test drops the last
// reference and checks that the socket was destroyed: a leaked message would
// still pin it, as it pins a tcp_trsp_socket in production.
//
// send_request() resolves its target through the resolver singleton, whose
// cache thread is stopped by test_resolver; run that suite after this one.

namespace {

class stub_socket : public trsp_socket {
  bool reliable;
  bool *destroyed;

public:
  std::string sent;
  int sends;

  stub_socket(bool reliable, bool *destroyed)
      : trsp_socket(0, 0), reliable(reliable), destroyed(destroyed), sends(0) {
    ip = "127.0.0.1";
    port = 5070;
  }

  ~stub_socket() override { *destroyed = true; }

  int bind(const string &, unsigned short) override { return 0; }
  const char *get_transport() const override { return reliable ? "tcp" : "udp"; }
  bool is_reliable() const override { return reliable; }

  int send(const sockaddr_storage *, const char *msg, const int msg_len, unsigned int) override {
    sent.assign(msg, msg_len);
    sends++;
    return 0;
  }
};

class capture_logger : public msg_logger {
public:
  std::string logged;

  int log(const char *buf, int len, sockaddr_storage *, sockaddr_storage *, cstring, int) override {
    logged.assign(buf ? buf : "", buf ? len : 0);
    return 0;
  }
};

sip_msg *parse_request(const std::string &raw) {
  sip_msg *msg = new sip_msg(raw.c_str(), raw.length());
  char *err_msg = 0;
  if (parse_sip_msg(msg, err_msg)) {
    delete msg;
    return NULL;
  }
  return msg;
}

// a separate call_id per test keeps any transaction state a test leaves
// behind from matching the next test's requests
std::string make_request(const char *method, const char *trsp, const char *call_id) {
  std::string via_trsp(trsp);
  for (size_t i = 0; i < via_trsp.size(); i++) {
    via_trsp[i] = toupper(via_trsp[i]);
  }

  std::string msg;
  msg += std::string(method) + " sip:bob@127.0.0.1:5099;transport=" + trsp + " SIP/2.0\r\n";
  msg += "Via: SIP/2.0/" + via_trsp + " 192.0.2.1:5060;branch=z9hG4bK776asdhds\r\n";
  msg += "To: <sip:bob@127.0.0.1>;tag=callee-tag\r\n";
  msg += "From: <sip:alice@192.0.2.1>;tag=caller-tag\r\n";
  msg += std::string("Call-ID: ") + call_id + "@192.0.2.1\r\n";
  msg += std::string("CSeq: 1 ") + method + "\r\n";
  msg += "Content-Length: 0\r\n";
  msg += "\r\n";
  return msg;
}

const unsigned int SEND_FLAGS = TR_FLAG_DISABLE_BL;

} // namespace

FCTMF_SUITE_BGN(test_trans_layer) {

  FCT_TEST_BGN(ack_2xx_over_reliable_transport_is_freed) {
    bool destroyed = false;
    stub_socket *sock = new stub_socket(true, &destroyed);
    inc_ref(sock);
    trans_layer::instance()->register_transport(sock);

    capture_logger *logger = new capture_logger();
    inc_ref(logger);

    sip_msg *ack = parse_request(make_request("ACK", "tcp", "reliable-ack"));
    fct_req(ack != NULL);

    trans_ticket tt;
    int res = trans_layer::instance()->send_request(ack, &tt, cstring(), cstring(), 0, SEND_FLAGS, logger);
    fct_chk_eq_int(res, 0);
    fct_chk_eq_int(sock->sends, 1);
    // a 2xx-ACK over TCP/TLS gets no transaction ...
    fct_chk(tt.get_trans() == NULL);
    // ... and is logged before it is freed
    fct_chk(!sock->sent.empty());
    fct_chk(logger->logged == sock->sent);

    delete ack;
    trans_layer::instance()->clear_transports();
    dec_ref(sock);
    // only a leaked copy of the ACK can still hold the socket
    fct_chk(destroyed);

    dec_ref(logger);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(ack_2xx_over_unreliable_transport_moves_buffer_into_invite) {
    bool destroyed = false;
    stub_socket *sock = new stub_socket(false, &destroyed);
    inc_ref(sock);
    trans_layer::instance()->register_transport(sock);

    capture_logger *logger = new capture_logger();
    inc_ref(logger);

    // the INVITE transaction the 2xx-ACK belongs to, as left behind by
    // a 200 OK with the callee's to-tag
    sip_msg *invite = parse_request(make_request("INVITE", "udp", "unreliable-ack"));
    fct_req(invite != NULL);
    trans_bucket *bucket = get_trans_bucket(invite->callid->value, get_cseq(invite)->num_str);
    bucket->lock();
    sip_trans *inv_t = bucket->add_trans(invite, TT_UAC);
    inv_t->to_tag.s = new char[10];
    memcpy((void *)inv_t->to_tag.s, "callee-tag", 10);
    inv_t->to_tag.len = 10;
    bucket->unlock();

    sip_msg *ack = parse_request(make_request("ACK", "udp", "unreliable-ack"));
    fct_req(ack != NULL);

    trans_ticket tt;
    int res = trans_layer::instance()->send_request(ack, &tt, cstring(), cstring(), 0, SEND_FLAGS, logger);
    fct_chk_eq_int(res, 0);
    fct_chk_eq_int(sock->sends, 1);
    fct_chk(tt.get_trans() == inv_t);
    // the INVITE transaction now owns the ACK for retransmissions
    fct_chk(!sock->sent.empty());
    fct_chk(inv_t->retr_buf && sock->sent == std::string(inv_t->retr_buf, inv_t->retr_len));
    fct_chk(inv_t->retr_socket == sock);
    fct_chk(logger->logged == sock->sent);

    bucket->lock();
    bucket->remove(inv_t);
    bucket->unlock();

    delete ack;
    trans_layer::instance()->clear_transports();
    dec_ref(sock);
    fct_chk(destroyed);

    dec_ref(logger);
  }
  FCT_TEST_END();

  FCT_TEST_BGN(ack_2xx_over_unreliable_transport_without_invite_is_freed) {
    bool destroyed = false;
    stub_socket *sock = new stub_socket(false, &destroyed);
    inc_ref(sock);
    trans_layer::instance()->register_transport(sock);

    sip_msg *ack = parse_request(make_request("ACK", "udp", "unmatched-ack"));
    fct_req(ack != NULL);

    // no INVITE transaction to hand the ACK to: send_request() fails
    // and has to free the message it generated
    trans_ticket tt;
    int res = trans_layer::instance()->send_request(ack, &tt, cstring(), cstring(), 0, SEND_FLAGS, NULL);
    fct_chk(res < 0);
    fct_chk_eq_int(sock->sends, 1);

    delete ack;
    trans_layer::instance()->clear_transports();
    dec_ref(sock);
    fct_chk(destroyed);
  }
  FCT_TEST_END();
}
FCTMF_SUITE_END();
