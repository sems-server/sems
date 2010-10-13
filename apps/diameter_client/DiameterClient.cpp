/*
 * Copyright (C) 2007-2009 IPTEGO GmbH
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
  if (tcp_init_tcp()) {
    ERROR("initializing tcp transport layer.\n");
    return -1;
  }

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
  int    req_timeout  = args.get(9).asInt();

  string ca_file;
  string cert_file;

  if (args.size() > 10) {
    ca_file = args.get(10).asCStr();
    cert_file = args.get(11).asCStr();
  }

  ServerConnection* sc = new ServerConnection();
  DBG("initializing new connection for application %s...\n",
      app_name.c_str());
  sc->init(server_ip, server_port, 
	   ca_file, cert_file,
	   origin_host, origin_realm, origin_ip, 
	   app_id, vendor_id, product_name, 
	   req_timeout);

  DBG("starting new connection...\n");
  sc->start();

  DBG("registering connection...\n");
  conn_mut.lock();
  connections.insert(make_pair(app_name, sc));
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
    if (args.size()==10 /*sizeof("ssisssiisi")*/) {
      args.assertArrayFmt("ssisssiisi");
    } else  {
      // plus optional ssl/tls parameters ss
      args.assertArrayFmt("ssisssiisiss"); 
    }
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
  } else if(method == "test1"){
    AmArg a; 
    a.push(AmArg("vtm"));
//     a.push(AmArg("10.1.0.196"));
//     a.push(AmArg(8080));
    a.push(AmArg("62.220.31.182"));
    a.push(AmArg(9381));
//     a.push(AmArg("127.0.0.1"));
//     a.push(AmArg(4433));
    a.push(AmArg("vtm01"));
    a.push(AmArg("vtm.t-online.de"));
    a.push(AmArg("10.42.32.13"));
    a.push(AmArg(16777241));
    a.push(AmArg(29631));
    a.push(AmArg("vtm"));
    a.push(AmArg(20)); // timeout
//     a.push(AmArg("ca.pem"));
//     a.push(AmArg("client.pem"));
    newConnection(a, ret);
  } else if(method == "test2"){
    AmArg a; 
#define AAA_APP_USPI    16777241
#define AVP_E164_NUMBER     1024
#define AAA_VENDOR_IPTEGO  29631
#define AAA_LAR         16777214

    a.push(AmArg("vtm"));
    a.push(AmArg(AAA_LAR));
    a.push(AmArg(AAA_APP_USPI));
    DBG("x pushin \n");
    AmArg avps;

    AmArg e164;
    e164.push((int)AVP_E164_NUMBER);
    e164.push((int)AAA_AVP_FLAG_VENDOR_SPECIFIC | AAA_AVP_FLAG_MANDATORY);
    e164.push((int)AAA_VENDOR_IPTEGO);
    string e164_number = "+49331600001";
    e164.push(ArgBlob(e164_number.c_str(), e164_number.length()));
    avps.push(e164);

    AmArg drealm;
    drealm.push((int)AVP_Destination_Realm);
    drealm.push((int)AAA_AVP_FLAG_MANDATORY);
    drealm.push((int)0);
    string dest_realm = "iptego.de";
    drealm.push(ArgBlob(dest_realm.c_str(), dest_realm.length()));
    avps.push(drealm);

    a.push(avps);
    a.push(AmArg("bogus_link"));

    // check...

    DBG("x checking\n");
    a.assertArrayFmt("siias");
    DBG("x checking\n");
    // check values
    AmArg& vals = a.get(3);
    for (size_t i=0;i<vals.size(); i++) {
      AmArg& row = vals.get(i);
      //    [int avp_id, int flags, int vendor, blob data]
      row.assertArrayFmt("iiib");
    }
    DBG("x sendrequest\n");
    sendRequest(a, ret);

  } else if(method == "_list"){ 
    ret.push(AmArg("newConnection"));
    ret.push(AmArg("sendRequest"));
    ret.push(AmArg("test1"));
    ret.push(AmArg("test2"));
  }  else
    throw AmDynInvoke::NotImplemented(method);
}
