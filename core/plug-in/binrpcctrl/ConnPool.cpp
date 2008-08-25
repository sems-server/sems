
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <binrpc.h>

#include "log.h"
#include "ConnPool.h"


#define SND_USOCK_TEMPLATE  "/tmp/sems_send_sock_XXXXXX" //TODO: configurable
#define MAX_RETRY_ON_ERR    5

#ifndef UNIX_PATH_MAX
#include <sys/un.h>
#define UNIX_PATH_MAX sizeof(((struct sockaddr_un *)0)->sun_path)
#endif


// The locks are of type 'DEFAULT', non recursive -> should not fail.
#define LOCK \
  if (pthread_mutex_lock(&mutex) != 0) { \
    ERROR("CRITICAL: failed to lock mutex: %s [%d].\n", strerror(errno), \
        errno); \
    abort(); \
  }
#define UNLOCK \
  if (pthread_mutex_unlock(&mutex) != 0) { \
    ERROR("CRITICAL: failed to unlock mutex: %s [%d].\n", strerror(errno), \
        errno); \
    abort(); \
  }
#define WAIT \
  if (pthread_cond_wait(&cond, &mutex) != 0) { \
    ERROR("CRITICAL: failed to wait on condition: %s [%d].\n", \
        strerror(errno), errno); \
    abort(); \
  }
#define WAKEUP \
  if (pthread_cond_signal(&cond) != 0) { \
    ERROR("CRITICAL: failed to signal on cond"); \
    abort(); \
  }
#define SAFE(instr_set) \
  do { \
    LOCK; \
    instr_set; \
    UNLOCK; \
  } while (0)

ConnPool::ConnPool(const string &target, unsigned size, brpc_tv_t ct_to) :
  cap(size),
  size(0),
  ct_timeout(ct_to),
  onwait(0)
{
  brpc_addr_t *addr;
  if (! (addr = brpc_parse_uri(target.c_str())))
    throw "failed to parse BINRPC URI `" + target + "': " + 
        string(brpc_strerror()) + ".";
  else
    txAddr = *addr;

  if (pthread_mutex_init(&mutex, 0) != 0)
    throw "failed to init mutex";
  if (pthread_cond_init(&cond, 0) != 0)
    throw "failed to init wait condition";
}

ConnPool::~ConnPool()
{
  int fd;

  cap = 0; // prevent making new connections;
  while (size) {
    if (0 <= (fd = get())) {
      destroy(fd);
    } else {
      ERROR("failed to destroy all connections (%s [%d]).\n", 
          strerror(errno), errno);
      break;
    }
  }

  pthread_mutex_destroy(&mutex);
  pthread_cond_destroy(&cond);
}

int ConnPool::get()
{
  int fd;

  LOCK;

  while (fdStack.empty()) {
    if (size < cap) {
      size ++; // inc it now, so that the cap is enforced
      UNLOCK;
      return new_conn();
    } else {
      onwait ++;
      INFO("%dth worker asking for connectio, put on wait. "
          "Need more capacity? (current: %d)\n", onwait, cap);
      WAIT;
      onwait --;
    }
  }
  fd = fdStack.top();
  fdStack.pop();

  UNLOCK;

  DBG("connection FD#%d aquired.\n", fd);
  return fd;
}

void ConnPool::release(int fd)
{
  assert(0 <= fd);

  LOCK;
  if (onwait && fdStack.empty())
    WAKEUP;
  fdStack.push(fd);
  UNLOCK;
  DBG("connection FD#%d released.\n", fd);
}

void ConnPool::destroy(int fd)
{
  brpc_addr_t addr;

  assert(0 <= fd);

  LOCK;
  if (BRPC_ADDR_DOMAIN(&txAddr) == PF_LOCAL) {
    addr = locAddrMap[fd];
    locAddrMap.erase(fd);
  } else {
    BRPC_ADDR_DOMAIN(&addr) = 0;
  }
  size --;
  UNLOCK;

  if (BRPC_ADDR_DOMAIN(&addr)) {
    INFO("closing FD#%d for %s.\n", fd, brpc_print_addr(&addr));
    if (unlink(BRPC_ADDR_UN(&addr)->sun_path) < 0) {
      ERROR("failed to remove unix socket file '%s': %s [%d].\n", 
        BRPC_ADDR_UN(&addr)->sun_path, strerror(errno), errno);
    }
  } else {
    INFO("closing FD#%d for %s.\n", fd, brpc_print_addr(&txAddr));
  }

  if (close(fd) < 0)
    ERROR("FD %d closed uncleanly: %s [%d].\n", fd, strerror(errno), errno);

  DBG("connection FD#%d destroyied.\n", fd);
}

int ConnPool::new_conn()
{
  int fd;
  brpc_addr_t locAddr;

  //if using local sockets, we need to bind to a local socket when sending
  //out requests, so that SER knows where to send back replies for our
  //requests (unlike INET layers PF_LOCAL+DGRAM based communication needs to
  //bind.the sending socket in order to have this socket receive replies)
  if (BRPC_ADDR_DOMAIN(&txAddr) == PF_LOCAL) {
    //There's a race the loop tries to work around:
    //1. mkstemp creates & opens a temp file. brpc_socket() (called
    //later), removes the temporary file (unlink) and opens a unix 
    //socket with the same name.
    //So, between unlink() and socket(), some other mkstemp could
    //"ocupy" the name.
    int errcnt = -1;
    do {
      if (MAX_RETRY_ON_ERR < ++errcnt) {
        ERROR("%dth consecutive failed attempt to create a local domain "
          "socket for sending req - giving up.\n", errcnt);
        fd = -1;
        goto end;
      } else if (1 < errcnt) { //the previous brpc_socket() attempt failed
        ERROR("failed to create BINRPC socket: %s [%d].\n", brpc_strerror(),
          brpc_errno);
      }

      char buff[UNIX_PATH_MAX];
      assert(sizeof(SND_USOCK_TEMPLATE) <= sizeof(buff));
      memcpy(buff, SND_USOCK_TEMPLATE, sizeof(SND_USOCK_TEMPLATE));
      int tmpFd = mkstemp(buff);
      if (tmpFd < 0) {
        ERROR("failed to create temporary file with template `%s': %s [%d].\n",
            SND_USOCK_TEMPLATE, strerror(errno), errno);
        continue;
      } else {
        close(tmpFd); // close the FD - only the modified buff is of worth
      }
      locAddr = txAddr; //copy domain, socket type
      memcpy(BRPC_ADDR_UN(&locAddr)->sun_path, buff, strlen(buff)+/*0-term*/1);
      BRPC_ADDR_LEN(&locAddr) = SUN_LEN(BRPC_ADDR_UN(&locAddr));
      DBG("creating temporary send socket bound to `%s'.\n", buff);
    } while ((fd = brpc_socket(&locAddr, /*blocking*/false, 
        /*named*/true)) < 0);
    /* TODO: permission, UID/GID of the created socket */
  } else {
    if ((fd = brpc_socket(&txAddr, /*blk*/false, /*named*/false)) < 0) {
      ERROR("failed to create BINRPC socket: %s [%d].\n", brpc_strerror(),
        brpc_errno);
      fd = -1;
      goto end;
    }
  }
  if (! brpc_connect(&txAddr, &fd, ct_timeout)) {
    ERROR("failed to connect to SER: %s [%d].\n", brpc_strerror(), brpc_errno);
    close(fd);
    fd = -1;
  }

end:
  if (fd < 0) {
    SAFE(size --);
  } else {
    if (BRPC_ADDR_DOMAIN(&txAddr) == PF_LOCAL) {
      // connect succeeded -> do the mapping
      SAFE(locAddrMap[fd] = locAddr);
    }
    DBG("connection FD#%d created.\n", fd);
  }
  return fd;
}
