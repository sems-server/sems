/*
 * $Id: ServerConnection.cpp 463 2007-09-28 11:54:19Z sayer $
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

#ifndef _DIAMETER_SERVER_CONNECTION_H
#define _DIAMETER_SERVER_CONNECTION_H

#include "diameter_client.h"

#include "AmThread.h"
#include "AmEventQueue.h"
#include "AmArg.h"

#include <string>
#include <vector>
#include <map>
#include <utility>
using std::string;
using std::vector;
using std::map;
using std::pair;

#define AF_IP4 1 // http://www.iana.org/assignments/address-family-numbers

enum {
  AAA_ERROR_NOTINIT    = -1,
  AAA_ERROR_NOCONN     = -2,
  AAA_ERROR_NOFREECONN = -3,
  AAA_ERROR_TIMEOUT    = -4,
  AAA_ERROR_MESSAGE    = -5,
  AAA_ERROR_COMM       = -6
};

struct DiameterRequestEvent 
  : public AmEvent
{
  int command_code;
  int app_id;
  AmArg val;
  string sess_link;

  enum { ID_NewRequest = 0 }; 
  DiameterRequestEvent(int command_code,
		       int app_id,
		       AmArg val,
		       string sess_link)
    : AmEvent(ID_NewRequest), command_code(command_code),
    app_id(app_id), val(val), sess_link(sess_link)
  {
  }
};

struct DiameterServerConnection {
  bool in_use;
  int sockfd;
  rd_buf_t rb;

  string origin_host;

  AAAMsgIdentifier h2h;
  AAAMsgIdentifier e2e;
  void terminate();

  void setIDs(AAAMessage* msg);

  DiameterServerConnection();
  ~DiameterServerConnection() {}
};

class ServerConnection 
: public AmThread,
  public AmEventQueue,
  public AmEventHandler
{
  struct timeval connect_ts;
  bool open;
  
  string server_name;
  int server_port;
  string origin_host;
  string origin_realm;
  string origin_ip;
  AAAApplicationId app_id;

  char origin_ip_address[2+4];// AF and address

  //  the client
  string product_name;
  uint32_t vendorID;
  
  DiameterServerConnection conn;

  map<unsigned int, pair<string, struct timeval> > 
    req_map;
  AmMutex req_map_mut;

  void openConnection();

  int addOrigin(AAAMessage* msg);

  int handleReply(AAAMessage* rep);
  int handleRequest(AAAMessage* req);
  AAAMessage* ReqEvent2AAAMessage(DiameterRequestEvent* re);
  AmArg AAAMessageAVPs2AmArg(AAAMessage* rep);

  static int addStringAVP(AAAMessage* msg, AAA_AVPCode avp_code, string& val, bool attail = false);
  static int addDataAVP(AAAMessage* msg, AAA_AVPCode avp_code, char* val, unsigned int len);
  static int addResultCodeAVP(AAAMessage* msg, AAAResultCode code);
  static int addGroupedAVP(AAA_AVP *avp, AAA_AVPCode avp_code, char* val, unsigned int len);

  // send request and get response  
  int sendRequest(AAAMessage* req, unsigned int& exe);
  void process(AmEvent*);
  void receive();
  void setRetryConnectLater();
 public:
  ServerConnection();
  ~ServerConnection();
  int init(const string& _server_name, 
	   int _server_port,
	   const string& _origin_host, 
	   const string& _origin_realm,
	   const string& _origin_ip,
	   AAAApplicationId _app_id,
	   unsigned int _vendorID,
	   const string& _product_name);

  bool is_open() { return open; }
  void run();
  void on_stop();
};

#endif
