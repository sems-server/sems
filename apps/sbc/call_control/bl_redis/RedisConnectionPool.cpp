#include "RedisConnectionPool.h"
#include "log.h"


RedisConnectionPool::RedisConnectionPool() 
  : total_connections(0), failed_connections(0),
    have_active_connection(false), try_connect(true)
{
}

RedisConnectionPool::~RedisConnectionPool() {

  //     redisFree(redis_context);
}

void RedisConnectionPool::set_config(string& server, unsigned int port, 
				     vector<unsigned int> timers,
				     unsigned int max_conn_wait) {
  redis_server = server;
  redis_port = port;
  retry_timers = timers;
  retry_index = 0;
  max_wait = max_conn_wait;
}

void RedisConnectionPool::add_connections(unsigned int count) {
  connections_mut.lock();
  failed_connections += count;
  total_connections += count;
  connections_mut.unlock();
  try_connect.set(true);
}

void RedisConnectionPool::returnConnection(redisContext* c) {
  connections_mut.lock();
  connections.push_back(c);
  size_t active_size = connections.size();
  have_active_connection.set(true);
  connections_mut.unlock();
  DBG("Now %zd active connections\n", active_size);
}

void RedisConnectionPool::returnFailedConnection(redisContext* c) {
  redisFree(c);
  connections_mut.lock();
  failed_connections++;
  unsigned int inactive_size = failed_connections;
  connections_mut.unlock();

  DBG("Now %u inactive connections\n", inactive_size);
  retry_index = 0;
  try_connect.set(true);

  // if this was the last active connection returned, alert waiting
  // threads so they get error back
  have_active_connection.set(true);
}

redisContext* RedisConnectionPool::getActiveConnection() {
  redisContext* res = NULL;
  while (NULL == res) {

    connections_mut.lock();
    if (connections.size()) {
      res = connections.front();
      connections.pop_front();
      have_active_connection.set(!connections.empty());
    }
    connections_mut.unlock();

    if (NULL == res) {
      // check if all connections broken -> return null
      connections_mut.lock();
      bool all_inactive = total_connections == failed_connections;
      connections_mut.unlock();

      if (all_inactive) {
	DBG("all connections inactive - returning NO connection\n");
	return NULL;
      }

      // wait until a connection is back
      DBG("waiting for an active connection to return\n");
      if (!have_active_connection.wait_for_to(max_wait)) {
	WARN("timeout waiting for an active connection (waited %ums)\n",
	     max_wait);
	break;
      }
    } else {
      DBG("got active connection [%p]\n", res);
    }
  }

  return res;
}


void RedisConnectionPool::run() {
  DBG("RedisConnectionPool thread starting\n");
  try_connect.set(true);

  while (true) {
    try_connect.wait_for();
    try_connect.set(false);
    while (true) {
      connections_mut.lock();
      unsigned int m_failed_connections = failed_connections;
      connections_mut.unlock();

      if (!m_failed_connections)
	break;

      // add connections until error occurs
      redisContext* context = redisConnect((char*)redis_server.c_str(), redis_port);
      if (!context->err) {
	DBG("successfully connected to server %s:%u [%p]\n",
	    redis_server.c_str(), redis_port, context);
	returnConnection(context);
	retry_index=0;
	connections_mut.lock();
	failed_connections--;
	connections_mut.unlock();
      } else {
	DBG("connection to %s%u failed: '%s'\n",
	    redis_server.c_str(), redis_port, context->errstr);
	redisFree(context);
	if (retry_timers.size()) {
	  DBG("waiting for retry %u ms (index %u)\n",
	      retry_timers[retry_index], retry_index);
	  usleep(retry_timers[retry_index]*1000);
	  if (retry_index<retry_timers.size()-1)
	    retry_index++;
	} else {
	  DBG("waiting for retry 50 ms\n");
	  usleep(50);
	}
      }
    }
  }
}

void RedisConnectionPool::on_stop() {

}


