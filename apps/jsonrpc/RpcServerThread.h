/*
 * $Id: ModMysql.cpp 1764 2010-04-01 14:33:30Z peter_lemenkov $
 *
 * Copyright (C) 2010 TelTech Systems Inc.
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

#ifndef _RpcServerThread_h_
#define _RpcServerThread_h_

#include "AmEvent.h"
#include "AmEventQueue.h"
#include "AmThread.h"
#include "RpcPeer.h"

class RpcServerThread 
: public AmThread, public AmEventQueue, public AmEventHandler 
{

  char rcvbuf[MAX_RPC_MSG_SIZE];

 public:
  RpcServerThread();
  ~RpcServerThread();

  void run();
  void on_stop();

  void process(AmEvent* event);
};

class RpcServerThreadpool 
{
  vector<RpcServerThread*> threads;
  vector<RpcServerThread*>::iterator t_it;
  AmMutex threads_mut;

 public:
  RpcServerThreadpool();
  ~RpcServerThreadpool();
  
  void dispatch(AmEvent* ev);
  void addThreads(unsigned int cnt);
};

#endif
