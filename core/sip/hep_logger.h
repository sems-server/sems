#ifndef _hep_logger_h_
#define _hep_logger_h_

#include "msg_logger.h"

#include <stdint.h>
#include <sys/socket.h>

/**
 * Sends SIP messages as HEPv3 packets over UDP to a capture server
 * (Homer, heplify-server, ...). RTP passed to the logger is skipped.
 */
class hep_logger
  : public msg_logger
{
  sockaddr_storage capt_addr;
  uint32_t capt_id;
  string corr_id;
  int sd; // shared by all loggers, not owned

public:
  hep_logger();

  /**
   * dest: "ip:port[;id=N]", IPv6 as "[addr]:port"
   * correlation_id: sent with every message, ties the legs of a call together
   */
  int init(const string& dest, const string& correlation_id = string());

  int log(const char* buf, int len,
	  sockaddr_storage* src_ip,
	  sockaddr_storage* dst_ip,
	  cstring method, int reply_code=0);
};

#endif
