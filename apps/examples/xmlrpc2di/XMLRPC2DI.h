/*
 * $Id: XMLRPC2DI.h 145 2006-11-26 00:01:18Z sayer $
 *
 * Copyright (C) 2007 iptego GmbH
 *
 * This file is part of sems, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For a license to use the sems software under conditions
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
#ifndef XMLRPC2DISERVER_H
#define XMLRPC2DISERVER_H

#include "XmlRpc.h"
using namespace XmlRpc;

#include "AmThread.h"
#include "AmApi.h"

#include <map>
using std::map;
using std::multimap;

#include <string>
using std::string;

#include <time.h>

#define DEF_XMLRPCSERVERMETHOD(cls_name, func_name)		\
  class cls_name						\
  :  public XmlRpcServerMethod {				\
								\
  public:							\
  cls_name(XmlRpcServer* s) :					\
    XmlRpcServerMethod(func_name, s) { }			\
								\
	void execute(XmlRpcValue& params, XmlRpcValue& result); \
  }


DEF_XMLRPCSERVERMETHOD(XMLRPC2DIServerCallsMethod,       "calls");
DEF_XMLRPCSERVERMETHOD(XMLRPC2DIServerSetLoglevelMethod, "set_loglevel");
DEF_XMLRPCSERVERMETHOD(XMLRPC2DIServerGetLoglevelMethod, "get_loglevel");

class XMLRPC2DIServerDIMethod 
:  public XmlRpcServerMethod { 
  
 public: 
  XMLRPC2DIServerDIMethod(XmlRpcServer* s) : 
    XmlRpcServerMethod("di", s) { } 

  void execute(XmlRpcValue& params, XmlRpcValue& result); 
};

struct DIMethodProxy : public XmlRpcServerMethod
{
  std::string di_method_name;
  std::string server_method_name;

  AmDynInvokeFactory* di_factory;

  DIMethodProxy(std::string const &server_method_name, 
		std::string const &di_method_name,
		AmDynInvokeFactory* di_factory);
  
  void execute(XmlRpcValue& params, XmlRpcValue& result);
};

class XMLRPC2DIServer : public AmThread {
  XmlRpcServer s;
  unsigned int port; 
  XMLRPC2DIServerCallsMethod       calls_method;
  XMLRPC2DIServerSetLoglevelMethod setloglevel_method;
  XMLRPC2DIServerGetLoglevelMethod getloglevel_method;
  XMLRPC2DIServerDIMethod*         di_method;
  void registerMethods(const std::string& iface);

 public: 
  XMLRPC2DIServer(unsigned int port, 
		  bool di_export, 
		  string direct_export);
  void run();
  void on_stop();
  
  static void xmlrpcval2amarg(XmlRpcValue& v, AmArg& a, 
			      unsigned int start_index = 0);

  /** convert all args in a into result*/
  static void amarg2xmlrpcval(const AmArg& a, XmlRpcValue& result);
};

class  XMLRPCServerEntry {
  bool active;
  time_t last_try;

 public: 
  XMLRPCServerEntry(string s, int p, string u);
  ~XMLRPCServerEntry();

  bool is_active();
  void set_failed();

  string server;
  int port;
  string uri;
};

class XMLRPC2DI 
: public AmDynInvokeFactory, 
  public AmDynInvoke {

  XMLRPC2DIServer* server;
  unsigned int XMLRPCPort;

  static XMLRPC2DI* _instance;
  bool configured;
  int load();

  //  app           server
  multimap<string, XMLRPCServerEntry*> servers;
  AmMutex server_mut;
  XMLRPCServerEntry* getServer(const string& app_name);

  void newConnection(const AmArg& args, AmArg& ret);
  void sendRequest(const AmArg& args, AmArg& ret);
 public:
  XMLRPC2DI(const string& mod_name);
  ~XMLRPC2DI() { }
  int onLoad();

  // DI factory
  AmDynInvoke* getInstance() { return instance(); }

  // DI API
  static XMLRPC2DI* instance();
  void invoke(const string& method, 
	      const AmArg& args, AmArg& ret);

  static unsigned int ServerRetryAfter;
};

#endif
