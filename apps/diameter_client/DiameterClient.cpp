/*
 * $Id$
 *
 * Copyright (C) 2007 iptego GmbH
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * sems is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "DiameterClient.h"

#include "log.h"
//#include <stdlib.h>

#define MOD_NAME "diameter_client"

#include <vector>
using std::vector;

EXPORT_PLUGIN_CLASS_FACTORY(DiameterClient, MOD_NAME);

DiameterClient* DiameterClient::_instance=0;

DiameterClient* DiameterClient::instance()
{
  if(_instance == NULL){
    _instance = new DiameterClient(MOD_NAME);
  }
  return _instance;
}

DiameterClient::DiameterClient(const string& name) 
  : AmDynInvokeFactory(name)
{
}

DiameterClient::~DiameterClient() {
}

int DiameterClient::onLoad() {
  DBG("DiameterClient loaded.\n");
  return 0;
}

void DiameterClient::newConnection(const AmArg& args, 
				   AmArg& ret) {
  string app_name     = args.get(0).asCStr();
  string server_ip    = args.get(1).asCStr();
  int    server_port  = args.get(2).asInt();
  string origin_host  = args.get(3).asCStr();
  string origin_realm = args.get(4).asCStr();
  string origin_ip    = args.get(5).asCStr();
  int    app_id       = args.get(6).asInt();
  int    vendor_id    = args.get(7).asInt();
  string product_name = args.get(8).asCStr();

  ServerConnection* sc = new ServerConnection();
  DBG("initializing new connection for application %s...\n",
      app_name.c_str());
  sc->init(server_ip, server_port, 
	   origin_host, origin_realm, origin_ip, 
	   app_id, vendor_id, product_name);
  DBG("starting new connection...\n");
  sc->start();
  DBG("registering connection...\n");
  conn_mut.lock();
  connections.insert(std::make_pair(app_name, sc));
  conn_mut.unlock();

  ret.push(0);
  ret.push("new connection registered");
  return;
}

void DiameterClient::sendRequest(const AmArg& args, 
				 AmArg& ret) {
  string app_name     = args.get(0).asCStr();
  int    command_code = args.get(1).asInt();
  int    app_id       = args.get(2).asInt();
  AmArg& val          = args.get(3);
  string sess_link    = args.get(4).asCStr();

  vector<ServerConnection*> scs;    
  conn_mut.lock();
  for (multimap<string, ServerConnection*>::iterator it=
	 connections.lower_bound(app_name);
       it != connections.upper_bound(app_name); it++) {
    if (it->second->is_open())
      scs.push_back(it->second);
  }
  conn_mut.unlock();

  DBG("found %zd active connections for application %s\n", 
      scs.size(), app_name.c_str());

  if (scs.empty()) {
    // no connections found
    ret.push(-1);
    ret.push("no active connections");
    return;
  }
  // select one connection randomly 
  size_t pos = random() % scs.size();
  scs[pos]->postEvent(new DiameterRequestEvent(command_code, app_id, 
					       val, sess_link));
  ret.push(0);
  ret.push("request sent");
  return;
}

void DiameterClient::invoke(const string& method, const AmArg& args, 
			    AmArg& ret)
{
  if(method == "newConnection"){
    args.assertArrayFmt("ssisssiis");
    newConnection(args, ret);
  } else if(method == "sendRequest"){
    args.assertArrayFmt("siias");
    // check values
    AmArg& vals = args.get(3);
    for (size_t i=0;i<vals.size(); i++) {
      AmArg& row = vals.get(i);
      //    [int avp_id, int flags, int vendor, blob data]
      row.assertArrayFmt("iiib");
    }
    sendRequest(args, ret);
  } else if(method == "_list"){ 
    ret.push(AmArg("newConnection"));
    ret.push(AmArg("sendRequest"));
  }  else
    throw AmDynInvoke::NotImplemented(method);
}
