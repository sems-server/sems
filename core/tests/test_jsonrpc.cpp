#include "fct.h"

#include "log.h"

#ifdef SEMS_TESTS_JSONRPC

#include "AmEvent.h"
#include "AmThread.h"

#include "../../apps/jsonrpc/JsonRPC.h"
#include "../../apps/jsonrpc/RpcServerLoop.h"
#include "../../apps/jsonrpc/RpcServerThread.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

// The JSON-RPC module runs an event loop thread, which accepts connections on
// a TCP port, and a pool of server threads, which process the messages. Both
// have to stop when asked to, whatever state the request finds them in, since
// the module is unloaded right after at shutdown.
//
// JsonRPC.cpp is not part of sems_tests, it exports the same factory symbol
// as uac_auth, so the two settings the server loop reads from it are defined
// here. The loop adds threads - 1 server threads to the pool when it starts.

int JsonRPCServerModule::port = 47080;
int JsonRPCServerModule::threads = 2;

namespace {

long long now_ms() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// @return whether the thread has left run() within msec
bool stopped_within(AmThread &t, long long msec) {
  long long deadline = now_ms() + msec;
  while (!t.is_stopped()) {
    if (now_ms() > deadline) {
      return false;
    }
    usleep(5000);
  }
  return true;
}

// connects to the server loop, retrying while it is not listening yet
// @return the socket, or -1 after msec
int connect_to_server(long long msec) {
  long long deadline = now_ms() + msec;
  struct sockaddr_in sa;
  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_port = htons(JsonRPCServerModule::port);
  sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  while (true) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
      return -1;
    }
    if (::connect(fd, (struct sockaddr *)&sa, sizeof(sa)) == 0) {
      return fd;
    }
    ::close(fd);
    if (now_ms() > deadline) {
      return -1;
    }
    usleep(10000);
  }
}

// @return whether the peer closed the connection within msec
bool closed_within(int fd, long long msec) {
  struct pollfd pfd = {fd, POLLIN, 0};
  if (poll(&pfd, 1, (int)msec) <= 0) {
    return false;
  }
  char c;
  return recv(fd, &c, 1, 0) <= 0;
}

} // namespace

FCTMF_SUITE_BGN(test_jsonrpc) {

  // a thread that has been asked to stop leaves run(), whether it is waiting
  // for events by then or has not even started
  FCT_TEST_BGN(server_thread_stops_when_asked) {
    for (int before_start = 0; before_start < 2; before_start++) {
      RpcServerThread *t = new RpcServerThread();
      if (before_start) {
        t->request_stop();
      }
      t->start();
      if (!before_start) {
        usleep(20000); // let it wait for events
        t->request_stop();
      }
      bool stopped = stopped_within(*t, 2000);
      fct_xchk(stopped, "stop requested %s start(): the server thread did not stop",
               before_start ? "before" : "after");
      if (stopped) {
        t->join();
        delete t;
      }
    }
  }
  FCT_TEST_END();

  // cleanup() stops and joins all threads of a pool; afterwards there is
  // nothing to dispatch to, and the event is dropped
  FCT_TEST_BGN(thread_pool_cleanup_stops_its_threads) {
    RpcServerThreadpool pool; // starts one thread
    pool.addThreads(2);
    usleep(20000);

    long long start = now_ms();
    pool.cleanup();
    fct_chk(now_ms() - start < 2000);

    pool.dispatch(new AmEvent(0));
    pool.cleanup();
  }
  FCT_TEST_END();

  // the loop accepts connections while it runs; once asked to stop, the loop
  // thread leaves run() and the connections are closed
  FCT_TEST_BGN(server_loop_stops_and_closes_its_connections) {
    JsonRPCServerLoop *loop = new JsonRPCServerLoop();
    loop->start();
    int client = connect_to_server(3000);
    fct_req(client >= 0);
    usleep(100000); // let the loop accept the connection

    loop->request_stop();
    bool stopped = stopped_within(*loop, 5000);
    fct_chk(stopped);
    fct_chk(closed_within(client, 2000));
    ::close(client);
    if (stopped) {
      loop->join();
      delete loop;
    }
  }
  FCT_TEST_END();

  // a stop requested while run() is still setting up, or before the thread
  // runs at all, must not be lost
  FCT_TEST_BGN(server_loop_stops_when_asked_around_start) {
    for (int before_start = 0; before_start < 2; before_start++) {
      JsonRPCServerLoop *loop = new JsonRPCServerLoop();
      if (before_start) {
        loop->request_stop();
      }
      loop->start();
      if (!before_start) {
        loop->request_stop();
      }
      bool stopped = stopped_within(*loop, 5000);
      fct_xchk(stopped, "stop requested %s start(): the loop thread did not stop",
               before_start ? "before" : "right after");
      if (stopped) {
        loop->join();
        delete loop;
      }
    }
  }
  FCT_TEST_END();
}
FCTMF_SUITE_END();

#else // SEMS_TESTS_JSONRPC

// built without libev: no JSON-RPC module to test
FCTMF_SUITE_BGN(test_jsonrpc) {}
FCTMF_SUITE_END();

#endif
