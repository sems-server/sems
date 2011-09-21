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

#ifndef _Rtmp_h_
#define _Rtmp_h_

#include "RtmpServer.h"
#include "AmApi.h"
#include "AmEventProcessingThread.h"

#define MOD_NAME "rtmp"
#define FACTORY_Q_NAME (MOD_NAME "_ev_proc")

class RtmpConnection;

struct RtmpConfig
{
  // RTMP server params
  string ListenAddress;
  unsigned int ListenPort;

  // Outbound call params
  string FromName;
  string FromDomain;

  // Registration related params
  bool   AllowExternalRegister;
  string ImplicitRegistrar;

  RtmpConfig();
};

class RtmpFactory
  : public AmSessionFactory,
    public AmEventProcessingThread
{
  // Global module configuration
  RtmpConfig cfg;

  // Container keeping trace of registered RTMP connections
  // to enable inbound calls to RTMP clients
  map<string,RtmpConnection*> connections;
  AmMutex                     m_connections;
  
  // registrar_client instance pointer
  AmDynInvoke* di_reg_client;

protected:

public:
  RtmpFactory();
  ~RtmpFactory();

  // from AmPluginFactory
  int onLoad();

  // from AmSessionFactory
  AmSession* onInvite(const AmSipRequest& req, const string& app_name,
		      const map<string,string>& app_params);

  const RtmpConfig* getConfig() { return &cfg; }
  AmDynInvoke* getRegClient() { return di_reg_client; }
  
  int addConnection(const string& ident, RtmpConnection*);
  void removeConnection(const string& ident);
};

// declare the RtmpFactory as a singleton
typedef singleton<RtmpFactory> RtmpFactory_impl;

#endif
