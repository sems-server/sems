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

#define DEF_XMLRPCSERVERMETHOD(cls_name, func_name) \
class cls_name \
	:  public XmlRpcServerMethod { \
\
public: \
	cls_name(XmlRpcServer* s) : \
		XmlRpcServerMethod(func_name, s) { } \
\
	void execute(XmlRpcValue& params, XmlRpcValue& result); \
};


DEF_XMLRPCSERVERMETHOD(XMLRPC2DIServerCallsMethod,       "calls") 
DEF_XMLRPCSERVERMETHOD(XMLRPC2DIServerSetLoglevelMethod, "set_loglevel") 
DEF_XMLRPCSERVERMETHOD(XMLRPC2DIServerGetLoglevelMethod, "get_loglevel") 

class XMLRPC2DIServerDIMethod 
       :  public XmlRpcServerMethod { 

public: 
	XMLRPC2DIServerDIMethod(XmlRpcServer* s) : 
		XmlRpcServerMethod("di", s) { } 

	void execute(XmlRpcValue& params, XmlRpcValue& result); 
	void add2result(const AmArg& a, XmlRpcValue& result, unsigned int pos);
};



     class XMLRPC2DIServer : public AmThread {
  XmlRpcServer s;
  unsigned int port; 
  XMLRPC2DIServerCallsMethod       calls_method;
  XMLRPC2DIServerSetLoglevelMethod setloglevel_method;
  XMLRPC2DIServerGetLoglevelMethod getloglevel_method;
  XMLRPC2DIServerDIMethod          di_method;

public: 
  XMLRPC2DIServer(unsigned int port);
  void run();
  void on_stop();
};


class XMLRPC2DI : public AmDynInvokeFactory {
  XMLRPC2DIServer* server;
  unsigned int XMLRPCPort;
 public:
  XMLRPC2DI(string mod_name);
  ~XMLRPC2DI() { }
  int onLoad();
  AmDynInvoke* getInstance() { return NULL; }

};

#endif
