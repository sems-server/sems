#ifndef _DRedisConnection_h_
#define _DRedisConnection_h_

#include "hiredis/hiredis.h"
#include "DBTypes.h"

#define DEFAULT_REDIS_HOST "127.0.0.1"
#define DEFAULT_REDIS_PORT 6379

#define DEFAULT_REDIS_CONNECT_TIMEOUT 500

struct DRedisConfig
{
  string       host;
  unsigned int port;
  bool         unix_socket;
  bool         full_logging;
  bool         use_transactions;
  struct timeval tv_timeout;

  DRedisConfig(const string& host = DEFAULT_REDIS_HOST,
	      unsigned int port = DEFAULT_REDIS_PORT,
	      bool unix_socket = false,
	      bool full_logging = false, 
	      bool use_transactions = false,
	      int connect_timeout = DEFAULT_REDIS_CONNECT_TIMEOUT);
};

class DRedisConnection
{
  DRedisConfig   cfg;
  redisContext* redis_context;

  int handle_redis_reply(redisReply *reply, const char* _cmd);

public:
  DRedisConnection(const DRedisConfig& cfg);
  ~DRedisConnection(); 

  bool connect();
  void disconnect();

  bool connected() { return redis_context != NULL; }

  int exec_cmd(const char* cmd, redisReply*& reply);
  int append_cmd(const char* cmd);
  int get_reply(redisReply*& reply);
};

#endif
