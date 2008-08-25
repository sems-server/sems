
#include "AmUtils.h"
#include "log.h"
#include "CtrlServer.h"

CtrlServer::CtrlServer(const string &listen, unsigned listeners,
    brpc_tv_t rx_timeout, brpc_tv_t tx_timeout) : 
  wcnt(listeners)
{
  brpc_addr_t *addr;

  if (! (addr = brpc_parse_uri(listen.c_str()))) {
    throw "failed to parse BINRPC URI `" + listen + "': " + 
        string(brpc_strerror()) + " [" + int2str(brpc_errno) + "]";
  } else if (BRPC_ADDR_TYPE(addr) != SOCK_DGRAM) {
    //b/c we'd have to do connection management otherwise (listen for
    //connections, poll for activity on each descriptor etc); for now, not
    //relly needed and the impl. is much easier.
    throw "only datagram listeners supported";
  } else {
    rxAddr = *addr;
  }

  if ((rxFd = brpc_socket(addr, /*blocking*/false, /*bind*/true)) < 0) 
    throw "failed to get listen socket for URI `" + listen + "': " + 
        string(brpc_strerror()) + " [" + int2str(brpc_errno) + "].\n";

  workers = new CtrlWorker[wcnt]();

  for (unsigned i = 0; i < wcnt; i ++)
    workers[i].init(rxFd, rxAddr, rx_timeout, tx_timeout);
}

CtrlServer::~CtrlServer()
{
  INFO("closing SEMS listener FD#%d for %s.\n", rxFd, 
      brpc_print_addr(&rxAddr));

  if (close(rxFd) < 0)
    ERROR("CtrlServer server socket#%d closed uncleanly: %s [%d].\n", rxFd, 
        strerror(errno), errno);
  
  if (BRPC_ADDR_DOMAIN(&rxAddr) == PF_LOCAL) {
    if (unlink(BRPC_ADDR_UN(&rxAddr)->sun_path) < 0) {
      ERROR("failed to remove unix socket file '%s': %s [%d].\n", 
        BRPC_ADDR_UN(&rxAddr)->sun_path, strerror(errno), errno);
    }
  }
  delete []workers;
}

void CtrlServer::start()
{
  for (unsigned i = 0; i < wcnt; i ++)
    workers[i].start();
  INFO("CtrlServer started.\n");
}

void CtrlServer::stop()
{
  INFO("CtrlServer stopping.\n");
  for (unsigned i = 0; i < wcnt; i ++)
    workers[i].stop();
}

void CtrlServer::join()
{
  for (unsigned i = 0; i < wcnt; i ++)
    workers[i].join();
  INFO("CtrlServer stopped.\n");
}


CtrlWorker::CtrlWorker() :
  rxFd(-1)
{}

void CtrlWorker::init(int rxFd, brpc_addr_t rxAddr, 
    brpc_tv_t rx_timeout, brpc_tv_t tx_timeout)
{
  this->rxFd = rxFd;
  this->rxAddr = rxAddr;
  this->rx_timeout = rx_timeout;
  this->tx_timeout = tx_timeout;
}

void CtrlWorker::run()
{
  brpc_addr_t from;
  brpc_t *req, *rpl;

  INFO("CtrlServer worker #%lx started.\n", pthread_self());
  running = 1;

  do {
    from = rxAddr; // avoid a syscall to find out socket type
    if (! (req = brpc_recvfrom(rxFd, &from, rx_timeout)))
      continue;
    //unsafe
    DBG("received BINRPC request `%.*s'.\n", BRPC_STR_FMT(brpc_method(req)));
    if ((rpl = brpc_cb_run(req))) {
      if (! brpc_sendto(rxFd, &from, rpl, tx_timeout)) {
        ERROR("failed to send reply to BINRPC request: %s [%d].\n",
          brpc_strerror(), brpc_errno);
      }
      brpc_finish(rpl);
    }
    brpc_finish(req);
  } while (running);
  
  INFO("CtrlServer worker #%lx stopped.\n", pthread_self());
}

void CtrlWorker::on_stop()
{
  running = 0;
}
