#ifndef _RtmpServer_h_
#define _RtmpServer_h_

#include "AmThread.h"
#include "singleton.h"

#include <poll.h>

#define MAX_CONNECTIONS   16
#define DEFAULT_RTMP_PORT 1935

class _RtmpServer
  : public AmThread
{
  struct pollfd fds[MAX_CONNECTIONS];
  unsigned int fds_num;

public:
  _RtmpServer();
  ~_RtmpServer();

protected:
  void run();
  void on_stop();
};

typedef singleton<_RtmpServer> RtmpServer;


#endif
