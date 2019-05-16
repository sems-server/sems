#include "DRedisConnection.h"
#include "jsonArg.h"
#include "AmUtils.h"

#include <errno.h>

DRedisConfig::DRedisConfig(const string& host, unsigned int port,
			 bool unix_socket, bool full_logging, 
			 bool use_transactions, int connect_timeout)
  : host(host), port(port), 
    unix_socket(unix_socket),
    full_logging(full_logging),
    use_transactions(use_transactions)
{
  tv_timeout.tv_sec = connect_timeout / 1000;
  tv_timeout.tv_usec = (connect_timeout - 1000 * tv_timeout.tv_sec) * 1000;
}

DRedisConnection::DRedisConnection(const DRedisConfig& cfg)
  : cfg(cfg), redis_context(NULL)
{
}

DRedisConnection::~DRedisConnection()
{
  disconnect();
}

void DRedisConnection::disconnect()
{
  if(redis_context) {
    DBG("disconnecting from Redis...");
    redisFree(redis_context);
    redis_context = NULL;
  }
}

bool DRedisConnection::connect()
{
  if(redis_context)
    return true;

  if(!cfg.unix_socket) {
    DBG("connecting to REDIS at %s:%u\n", cfg.host.c_str(), cfg.port);
    redis_context = redisConnectWithTimeout((char*)cfg.host.c_str(),
					    cfg.port, cfg.tv_timeout);
  }
  else {
    DBG("connecting to REDIS at %s\n", cfg.host.c_str());
    redis_context = redisConnectUnixWithTimeout(cfg.host.c_str(),
						cfg.tv_timeout);
  }

  if (redis_context->err) {
    ERROR("REDIS Connection error: %s\n", redis_context->errstr);
    disconnect();
    return false;
  }

  return true;
}

int DRedisConnection::handle_redis_reply(redisReply *reply, const char* _cmd) {
  if (!reply)  {
    switch (redis_context->err) {
    case REDIS_ERR_IO:
      ERROR("I/O error: %s (%s)\n", redis_context->errstr,_cmd);
      disconnect();
      return DB_E_CONNECTION;

    case REDIS_ERR_EOF: // silently reconnect
    case REDIS_ERR_OTHER: 
      disconnect();
      return DB_E_CONNECTION;

    case REDIS_ERR_PROTOCOL: 
      ERROR("REDIS Protocol error detected\n");
      disconnect();
      return DB_E_CONNECTION;
    }    
  }

  switch (reply->type) {
  case REDIS_REPLY_ERROR:
    ERROR("REDIS %s ERROR: %s\n", _cmd, reply->str);
    return DB_E_WRITE;

  case REDIS_REPLY_STATUS:
  case REDIS_REPLY_STRING:
    if (reply->len>=0) {
      if (cfg.full_logging) {
	DBG("REDIS %s: str: %.*s\n", _cmd, reply->len, reply->str); 
      }
    } break;

  case REDIS_REPLY_INTEGER:
    if (cfg.full_logging) {
      DBG("REDIS %s: int: %lld\n", _cmd, reply->integer);
    } break;

  case REDIS_REPLY_ARRAY: {
    if (cfg.full_logging) {
      DBG("REDIS %s: array START\n", _cmd);
    };
    for (size_t i=0;i<reply->elements;i++) {
      switch(reply->element[i]->type) {
      case REDIS_REPLY_ERROR: ERROR("REDIS %s ERROR: %.*s\n",
				    _cmd, reply->element[i]->len,
				    reply->element[i]->str);
	return DB_E_WRITE;

      case REDIS_REPLY_INTEGER:
	if (cfg.full_logging) {
	  DBG("REDIS %s: %lld\n", _cmd, reply->element[i]->integer);
	} break;

      case REDIS_REPLY_NIL: 
	if (cfg.full_logging) {
	  DBG("REDIS %s: nil\n", _cmd);
	} break;

      case REDIS_REPLY_ARRAY: 
	if (cfg.full_logging) {
	  DBG("REDIS : %zd elements\n", reply->elements);
	} break;

      case REDIS_REPLY_STATUS:
      case REDIS_REPLY_STRING:
	if (cfg.full_logging) {
	  if (reply->element[i]->len >= 0) {
	    DBG("REDIS %s: %.*s\n", _cmd,
		reply->element[i]->len, reply->element[i]->str); 
	  }
	}
	break;
      default:
	ERROR("unknown REDIS reply %d to %s!",reply->element[i]->type,  _cmd); break;
      }
    }
    if (cfg.full_logging) {
      DBG("REDIS %s: array END\n", _cmd);
    };
  }; break;

  default: ERROR("unknown REDIS reply %d to %s!", reply->type, _cmd); break;
  }

  if (cfg.full_logging) {
    DBG("REDIS cmd %s executed successfully\n", _cmd);
  }
  return DB_E_OK;
}

#define RETURN_READ_ERROR			\
  freeReplyObject(reply);			\
  return DB_E_READ;

int DRedisConnection::exec_cmd(const char* cmd, redisReply*& reply) {

  if(!redis_context) {
    ERROR("REDIS cmd '%s': not connected",cmd);
    return DB_E_CONNECTION;
  }
  reply = NULL;

  reply = (redisReply *)redisCommand(redis_context, cmd);
  int ret = handle_redis_reply(reply, cmd);
  if (ret != DB_E_OK)
    return ret;

  DBG("successfully executed redis cmd\n");
  return DB_E_OK;
}

int DRedisConnection::append_cmd(const char* cmd) {
  if(!redis_context) {
    ERROR("REDIS append cmd '%s': not connected",cmd);
    return DB_E_CONNECTION;
  }
  return redisAppendCommand(redis_context, cmd) == REDIS_OK ?
    DB_E_OK : DB_E_CONNECTION;
}

int DRedisConnection::get_reply(redisReply*& reply) {
  if(!redis_context) {
    ERROR("REDIS get_reply: not connected");
    return DB_E_CONNECTION;
  }

  redisGetReply(redis_context, (void**)&reply);
  int ret = handle_redis_reply(reply, "<pipelined>");
  return ret;  
}

