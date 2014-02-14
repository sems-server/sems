#ifndef _RedisConnectionPool_h_
#define _RedisConnectionPool_h_

#include "hiredis/hiredis.h"
#include "AmThread.h"

#include <string>
#include <list>
#include <vector>

using std::string;
using std::list;
using std::vector;

class RedisConnectionPool
: public AmThread
{

  list<redisContext*> connections;
  unsigned int total_connections;
  unsigned int failed_connections;
  AmMutex connections_mut;

  AmCondition<bool> have_active_connection;
  AmCondition<bool> try_connect;

  vector<unsigned int> retry_timers;
  unsigned int retry_index;

  string redis_server;
  unsigned int redis_port;
  unsigned int max_wait;  

 public:
  RedisConnectionPool();
  ~RedisConnectionPool();

  redisContext* getActiveConnection();
  
  void returnConnection(redisContext* c);

  void returnFailedConnection(redisContext* c);

  void set_config(string& server, unsigned int port, 
		  vector<unsigned int> timers, unsigned int max_conn_wait);

  void add_connections(unsigned int count);

  void run();
  void on_stop();
};

#endif
