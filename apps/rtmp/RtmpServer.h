/*
 * Copyright (C) 2011 Raphael Coeffic
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. This program is released under
 * the GPL with the additional exemption that compiling, linking,
 * and/or using OpenSSL is allowed.
 *
 * For a license to use the SEMS software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * SEMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _RtmpServer_h_
#define _RtmpServer_h_

#include "AmThread.h"
#include "singleton.h"

#include <sys/socket.h>
#include <poll.h>

#define MAX_CONNECTIONS   16
#define DEFAULT_RTMP_PORT 1935

class _RtmpServer
  : public AmThread
{
  sockaddr_storage listen_addr;
  struct pollfd fds[MAX_CONNECTIONS];
  unsigned int fds_num;

public:
  _RtmpServer();
  ~_RtmpServer();

  int listen(const char* ip, unsigned short port);

protected:
  void run();
  void on_stop();
  void dispose();
};

typedef singleton<_RtmpServer> RtmpServer;


#endif
