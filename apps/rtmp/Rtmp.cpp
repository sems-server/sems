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

#include "Rtmp.h"
#include "RtmpSession.h"
#include "RtmpConnection.h"
#include "AmConfigReader.h"
#include "AmEventDispatcher.h"
#include "ampi/SIPRegistrarClientAPI.h"

RtmpConfig::RtmpConfig()
  : ListenAddress("0.0.0.0"),
    ListenPort(DEFAULT_RTMP_PORT),
    FromName("RTMP Gateway"),
    FromDomain(),
    AllowExternalRegister(false),
    ImplicitRegistrar()
{
}

extern "C" void* FACTORY_SESSION_EXPORT()
{
  return RtmpFactory_impl::instance();
}

RtmpFactory::RtmpFactory()
  : AmSessionFactory(MOD_NAME),
    di_reg_client(NULL)
{
}

RtmpFactory::~RtmpFactory()
{
}

int RtmpFactory::onLoad()
{
  AmConfigReader cfg_file;

  if(cfg_file.loadPluginConf(MOD_NAME) < 0){
    INFO("No config file for " MOD_NAME " plug-in: using defaults.\n");
  }
  else {

    if(cfg_file.hasParameter("listen_address")){
      cfg.ListenAddress = fixIface2IP(cfg_file.getParameter("listen_address"),false);
    }

    if(cfg_file.hasParameter("listen_port")){
      string listen_port_str = cfg_file.getParameter("listen_port");
      if(sscanf(listen_port_str.c_str(),"%u",
		&(cfg.ListenPort)) != 1){
	ERROR("listen_port: invalid RTMP port specified (%s), using default\n",
	      listen_port_str.c_str());
	cfg.ListenPort = DEFAULT_RTMP_PORT;
      }
    }

    if(cfg_file.hasParameter("from_name")){
      cfg.FromName = cfg_file.getParameter("from_name");
    }

    if(cfg_file.hasParameter("from_domain")){
      cfg.FromDomain = cfg_file.getParameter("from_domain");
    }

    if(cfg_file.hasParameter("allow_external_register")){
      cfg.AllowExternalRegister = 
	cfg_file.getParameter("allow_external_register") == string("yes");
    }

    if(cfg_file.hasParameter("implicit_registrar")){
      cfg.ImplicitRegistrar = cfg_file.getParameter("implicit_registrar");
    }
  }

  RtmpServer* rtmp_server = RtmpServer::instance();
  
  if(rtmp_server->listen(cfg.ListenAddress.c_str(),cfg.ListenPort) < 0) {
    ERROR("could not start RTMP server at <%s:%u>\n",
	  cfg.ListenAddress.c_str(),cfg.ListenPort);
    rtmp_server->dispose();
    return -1;
  }
  rtmp_server->start();
  
  AmDynInvokeFactory* di_reg_client_f = AmPlugIn::instance()->
    getFactory4Di("registrar_client");
  if(di_reg_client_f)
    di_reg_client = di_reg_client_f->getInstance();

  if(di_reg_client) {
    // start the event processing
    AmEventDispatcher::instance()->addEventQueue(FACTORY_Q_NAME,this);
    start(); 
  }
  else {
    INFO("'registrar_client' not found: registration disabled.\n");
  }

  return 0;
}

AmSession* RtmpFactory::onInvite(const AmSipRequest& req, 
				 const string& app_name,
				 const map<string,string>& app_params)
{
  RtmpSession* sess=NULL;

  m_connections.lock();
  map<string,RtmpConnection*>::iterator it = connections.find(req.user);
  if(it != connections.end()){
    sess = new RtmpSession(it->second);
    it->second->setSessionPtr(sess);
    m_connections.unlock();
  }
  else {
    m_connections.unlock();
    AmSipDialog::reply_error(req,404,"Not found");
  }
  
  return sess;
}

int RtmpFactory::addConnection(const string& ident, RtmpConnection* conn)
{
  int res = 0;

  m_connections.lock();
  if(ident.empty() || (connections.find(ident)!=connections.end())){
    res = -1;
  }
  else {
    connections[ident] = conn;
  }
  m_connections.unlock();
  
  return res;
}

void RtmpFactory::removeConnection(const string& ident)
{
  m_connections.lock();
  connections.erase(ident);
  m_connections.unlock();
}
