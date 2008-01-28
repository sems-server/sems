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

#include "ServerConnection.h"
#include "AmSessionContainer.h"
#include "ampi/DiameterClientAPI.h"
#include "diameter_client.h"

#include <stdlib.h>
#include <string.h>
#include "log.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define CONN_WAIT_MAX   4      // 10  (*50ms = 0.5s)
#define CONN_WAIT_USECS 50000  // 50 ms

#define MAX_RETRANSMIT_RCV_RETRY 4

// #define EXTRA_DEBUG 

#define CONNECT_CEA_REPLY_TIMEOUT 2 // seconds
#define RETRY_CONNECTION_INTERVAL 2 // seconds

DiameterServerConnection::DiameterServerConnection()  
  : in_use(false), sockfd(-1) { 
  memset(&rb, 0, sizeof(rd_buf_t));
  h2h = random();
  e2e = (time(NULL) & 0xFFF << 20) | (random() % 0xFFFFF);
}


void DiameterServerConnection::terminate() {
  if (sockfd>0)
    close_tcp_connection(sockfd);

  sockfd = -1;
}

void DiameterServerConnection::setIDs(AAAMessage* msg) {
  msg->endtoendId = e2e++;
  msg->hopbyhopId = h2h++;
}

void ServerConnection::process(AmEvent* ev) {
  DiameterRequestEvent* re = dynamic_cast<DiameterRequestEvent*>(ev);
  if (NULL == re) {
    ERROR("received Event with wrong type!\n");
    return;
  }
  DBG(" making new request\n");

  AAAMessage* req = ReqEvent2AAAMessage(re);

  // end2end id, used to correlate req/reply
  unsigned int exe;

  if (sendRequest(req, exe)) {
    ERROR("sending request\n");
    return;
  }

  DBG("sent request with ID %d\n", exe);
  struct timeval now;
  gettimeofday(&now, NULL);

  req_map_mut.lock();
  req_map[exe] = std::make_pair(re->sess_link, now);
  req_map_mut.unlock();
}

ServerConnection::ServerConnection() 
  : server_port(-1), open(false),
    AmEventQueue(this)
{
}

ServerConnection::~ServerConnection() {
  DBG("closing diameter server connection.\n");
  conn.terminate();
}

int ServerConnection::init(const string& _server_name, 
			 int _server_port,
			 const string& _origin_host, 
			 const string& _origin_realm,
			 const string& _origin_ip,
			 AAAApplicationId _app_id,
			 unsigned int _vendorID,
			 const string& _product_name) {
  server_name = _server_name;
  server_port = _server_port;
  origin_host = _origin_host;
  origin_realm = _origin_realm;
  origin_ip = _origin_ip;
  product_name = _product_name;
  app_id = htonl(_app_id);
  // todo: separate vendor for client/app
  vendorID = htonl(_vendorID);

  memset(origin_ip_address, 0, sizeof(origin_ip_address));
  origin_ip_address[0] = 0;
  origin_ip_address[1] = AF_IP4;

  struct in_addr inp;
  if (inet_aton(origin_ip.c_str(), &inp) == 0) {
    ERROR("origin_ip %s could not be decoded.\n", 
	  origin_ip.c_str());
  } else {
    origin_ip_address[2] =  inp.s_addr & 0xFF;
    origin_ip_address[3] = (inp.s_addr & 0xFF00)     >> 8;
    origin_ip_address[4] = (inp.s_addr & 0xFF0000)   >> 16;
    origin_ip_address[5] = (inp.s_addr & 0xFF000000) >> 24;
  }

  // set connect_ts to past so it try to connect
  memset(&connect_ts, 0, sizeof(struct timeval));

  return 0;
}

void ServerConnection::openConnection() {
  DBG("init TCP connection\n");
  int res = init_mytcp(server_name.c_str(), server_port);
  if (res < 0) {
    ERROR("establishing connection to %s\n", 
	  server_name.c_str());
    setRetryConnectLater();
    return;
  }
  conn.sockfd = res;

  // send CER
  AAAMessage* cer;
  if ((cer=AAAInMessage(AAA_CC_CER, AAA_APP_DIAMETER_COMMON_MSG))==NULL) {
    ERROR(M_NAME":openConnection(): can't create new "
	  "CER AAA message!\n");
    conn.terminate();
    setRetryConnectLater();
    return;
  }
  if (addOrigin(cer) 
      || addDataAVP(cer, AVP_Host_IP_Address, origin_ip_address, sizeof(origin_ip_address)) 
      || addDataAVP(cer, AVP_Vendor_Id, (char*)&vendorID, sizeof(vendorID)) 
      || addDataAVP(cer, AVP_Supported_Vendor_Id, (char*)&vendorID, sizeof(vendorID)) 
      || addStringAVP(cer, AVP_Product_Name, product_name)) {
    ERROR("openConnection(): adding AVPs failed\n");
    conn.terminate();
    setRetryConnectLater();
    return;
  }

  // supported applications
  AAA_AVP* vs_appid;
  if( (vs_appid=AAACreateAVP(AVP_Vendor_Specific_Application_Id, (AAA_AVPFlag)AAA_AVP_FLAG_NONE, 0, 0, 
			     0, AVP_DONT_FREE_DATA)) == 0) {
    ERROR( M_NAME":openConnection(): creating AVP failed."
	   " (no more free memory!)\n");
    conn.terminate();
    setRetryConnectLater();
    return;
  }

  // feels like c coding...
  if (addGroupedAVP(vs_appid, AVP_Auth_Application_Id, 
		    (char*)&app_id, sizeof(app_id)) ||
      addGroupedAVP(vs_appid, AVP_Vendor_Id, 
		    (char*)&vendorID, sizeof(vendorID)) ||
      (AAAAddAVPToMessage(cer, vs_appid, 0) != AAA_ERR_SUCCESS)
      ) {
    ERROR( M_NAME":makeConnections(): creating AVP failed."
	   " (no more free memory!)\n");
    conn.terminate();
    setRetryConnectLater();
    return;
  }

#ifdef EXTRA_DEBUG 
  AAAPrintMessage(cer);
#endif

  conn.setIDs(cer);
  
  if(AAABuildMsgBuffer(cer) != AAA_ERR_SUCCESS) {
    ERROR( " makeConnections(): message buffer not created\n");
    AAAFreeMessage(&cer);
    return;
  }
  
  int ret = tcp_send(conn.sockfd, cer->buf.s, cer->buf.len);
  if (ret) {
    ERROR( "openConnection(): could not send message\n");
    conn.terminate();
    setRetryConnectLater();
    AAAFreeMessage(&cer);
    return;
  }
  
  AAAMessage* cea = NULL;
  res = tcp_recv_reply(conn.sockfd, &conn.rb, &cea, 
		       CONNECT_CEA_REPLY_TIMEOUT, 0);
  if (res) {
    ERROR( " makeConnections(): did not receive CEA reply.\n");
    conn.terminate();
    setRetryConnectLater();
    AAAFreeMessage(&cer);
    return;
  }
  
#ifdef EXTRA_DEBUG 
  if (cea != NULL)
    AAAPrintMessage(cea);
#endif
  
  DBG("Connection opened.\n");
  open = true;
}

AAAMessage* ServerConnection::ReqEvent2AAAMessage(DiameterRequestEvent* re) {
  AAAMessage* req = AAAInMessage(re->command_code, re->app_id);
  if (req == NULL) {
    ERROR("creating new request message.\n");
    return NULL;
  }
  
  for (int i=re->val.size()-1;i >= 0; i--) {
    //[int avp_id, int flags, int vendor, int len, blob data]
    
    AmArg& row = re->val.get(i);
    int avp_id    = row.get(0).asInt();
    int flags     = row.get(1).asInt();
    int vendor    = row.get(2).asInt();
    ArgBlob* data = row.get(3).asBlob();


#ifdef EXTRA_DEBUG
    DBG("adding avp id %d, flags %d, vendor %d\n", avp_id, flags, vendor);
#endif 

    if (!data->len) {
#ifdef EXTRA_DEBUG
      DBG("skipping empty AVP id %d\n", avp_id);
#endif 
      continue;
    }

    AAA_AVP *avp;
    if( (avp=AAACreateAVP(avp_id, (AAA_AVPFlag)flags, vendor, data->data,
			  data->len, AVP_DUPLICATE_DATA)) == 0) {
      ERROR( M_NAME ": addDataAVP() no more free memory!\n");
      continue;
    }
    
    if( AAAAddAVPToMessage(req, avp, 0)!= AAA_ERR_SUCCESS) {
      ERROR( M_NAME ": addDataAVP(): AVP not added!\n");
      continue;
    }
  }
  return req;
}

// add origin host and origin realm
int ServerConnection::addOrigin(AAAMessage* msg) {
  return addStringAVP(msg, AVP_Origin_Host, origin_host, true) || 
    addStringAVP(msg, AVP_Origin_Realm, origin_realm, true);
}


int ServerConnection::addStringAVP(AAAMessage* msg, AAA_AVPCode avp_code, string& val, 
				   bool attail) {
  AAA_AVP *avp;
  if( (avp=AAACreateAVP(avp_code, (AAA_AVPFlag)AAA_AVP_FLAG_NONE, 0, val.c_str(),
			val.length(), AVP_DUPLICATE_DATA)) == 0) {
    ERROR( M_NAME ": addStringAVP() no more free memory!\n");
    return -1;
  }

  AAA_AVP *pos = 0;
  if (attail)
    pos = msg->avpList.tail;

  if( AAAAddAVPToMessage(msg, avp, pos)!= AAA_ERR_SUCCESS) {
    ERROR( M_NAME ": addStringAVP(): AVP not added!\n");
    return -1;
  }
  return 0;
}

int ServerConnection::addResultCodeAVP(AAAMessage* msg, AAAResultCode code) {
  uint32_t n_code = htonl(code);
  return addDataAVP(msg, AVP_Result_Code, (char*)&n_code, sizeof(n_code));
}

int ServerConnection::addDataAVP(AAAMessage* msg, AAA_AVPCode avp_code, char* val, unsigned int len) {
   AAA_AVP *avp;
   if( (avp=AAACreateAVP(avp_code, (AAA_AVPFlag)AAA_AVP_FLAG_NONE, 0, val,
			 len, AVP_DUPLICATE_DATA)) == 0) {
     ERROR( M_NAME ": addDataAVP() no more free memory!\n");
     return -1;
   }

  if( AAAAddAVPToMessage(msg, avp, 0)!= AAA_ERR_SUCCESS) {
    ERROR( M_NAME ": addDataAVP(): AVP not added!\n");
    return -1;
  }
  return 0;
}

// add a new group member AVP
int ServerConnection::addGroupedAVP(AAA_AVP *avp, AAA_AVPCode avp_code, 
				  char* val, unsigned int len) {
  AAA_AVP *m_avp;

  if( (m_avp=AAACreateAVP(avp_code, (AAA_AVPFlag)AAA_AVP_FLAG_NONE, 0, val, 
			  len, AVP_DUPLICATE_DATA)) == 0) {
    ERROR( M_NAME":addGroupedAVP(): no more free memory!\n");
    return -1;
  }
  AAAAddGroupedAVP(avp, m_avp);
  return 0;
}

// send request and get response  
int ServerConnection::sendRequest(AAAMessage* req, unsigned int& exe) {
  if (addOrigin(req)) {
    return AAA_ERROR_MESSAGE;
  }

  conn.setIDs(req);

  if(AAABuildMsgBuffer(req) != AAA_ERR_SUCCESS) {
    ERROR( " sendRequest(): message buffer not created\n");
    return AAA_ERROR_MESSAGE;
  }
  
  int ret = tcp_send(conn.sockfd, req->buf.s, req->buf.len);
  if (ret) {
    ERROR( " sendRequest(): could not send message\n");
    AAAFreeMessage(&req);
    return AAA_ERROR_COMM;
  }

  exe = req->endtoendId;

  DBG("msg sent...\n");
  return 0;
}

int ServerConnection::handleRequest(AAAMessage* req) {
  switch (req->commandCode) {
  case AAA_CC_DWR: { // Device-Watchdog-Request
    DBG("Device-Watchdog-Request received\n");

    AAAMessage* reply;
    if ( (reply=AAAInMessage(AAA_CC_DWA, AAA_APP_DIAMETER_COMMON_MSG))==NULL) {
      ERROR(M_NAME":handleRequest(): can't create new "
	    "DWA message!\n");
      return -1;
    }

    AAAMessageSetReply(reply);
    
    if (addOrigin(reply) || addResultCodeAVP(reply, AAA_SUCCESS)) {
      return AAA_ERROR_MESSAGE;
    }

    reply->endtoendId = req->endtoendId;
    reply->hopbyhopId = req->hopbyhopId;

    if(AAABuildMsgBuffer(reply) != AAA_ERR_SUCCESS) {
      ERROR( " sendRequest(): message buffer not created\n");
      AAAFreeMessage(&reply);
      return AAA_ERROR_MESSAGE;
    }
    
    DBG("sending Device-Watchdog-Answer...\n");
    int ret = tcp_send(conn.sockfd, reply->buf.s, reply->buf.len);
    if (ret) {
      ERROR( " sendRequest(): could not send message\n");
      open = false;
      AAAFreeMessage(&reply);
      return AAA_ERROR_COMM;
    }
    AAAFreeMessage(&reply);
    return 0;

  }; break;

  case AAA_CC_DPR: { // Disconnect-Peer-Request
    DBG("Disconnect-Peer-Request not yet implemented\n");
  }; break;

  default: {
    ERROR("ignoring unknown request with command code %i\n", 
	  req->commandCode);
  }; break;
  }

  return 0;
}

int ServerConnection::handleReply(AAAMessage* rep) {
  unsigned int rep_id = rep->endtoendId;
  DBG("received reply - id %d\n", rep_id);

  string sess_link  = "";
  req_map_mut.lock();
  map<unsigned int, pair<string, struct timeval> >::iterator it =  
    req_map.find(rep_id);
  if (it != req_map.end()) {
    sess_link = it->second.first;
    req_map.erase(it);
  } else {
    DBG("session link for reply not found\n");
  }
  req_map_mut.unlock();

  if (!sess_link.empty()) {
    DiameterReplyEvent* r_ev = 
      new DiameterReplyEvent((unsigned int)rep->commandCode, (unsigned int)rep->applicationId, 
			     AAAMessageAVPs2AmArg(rep));
    if (!AmSessionContainer::instance()->postEvent(sess_link, r_ev)) {
      DBG("unhandled reply\n");
    }    
  }

  return 0;
}

AmArg ServerConnection::AAAMessageAVPs2AmArg(AAAMessage* rep) {
  AmArg res;
  for(AAA_AVP* avp=rep->avpList.head;avp;avp=avp->next) {
    AmArg a_avp;
    a_avp.push((int)avp->code);
    a_avp.push((int)avp->flags);
    a_avp.push((int)avp->vendorId);
    a_avp.push((int)avp->type);
    a_avp.push(ArgBlob(avp->data.s, avp->data.len));
    res.push(a_avp);
  }
  return res;
}

void ServerConnection::setRetryConnectLater() {
  gettimeofday(&connect_ts, NULL);
  connect_ts.tv_sec += RETRY_CONNECTION_INTERVAL;
}

void ServerConnection::on_stop() {
  DBG("todo: stop connection.\n");
}

void ServerConnection::receive() {
  AAAMessage *response = NULL;
  int res = tcp_recv_reply(conn.sockfd, &conn.rb, &response, 
			   0, CONN_WAIT_USECS);
  if (res) {
    ERROR( " receive(): tcp_recv_reply() failed.\n");
    open = false;
  }

  // nothing received
  if (response == NULL) 
    return;

    
#ifdef EXTRA_DEBUG 
  AAAPrintMessage(response);
#endif
  
  if (is_req(response)) 
    handleRequest(response);
  else 
    handleReply(response);
  
  AAAFreeMessage(&response);  
}

void ServerConnection::run() {
  DBG("running server connection\n");
  while (true) {
    if (!open) {
      struct timeval now;
      gettimeofday(&now, NULL);
      
      if(timercmp(&now,&connect_ts,>)){
	DBG("(re)trying to open the connection\n");
	openConnection();
      } else {
	usleep(50000);
      }
    } else {
      receive();
    }

    processEvents();
  }
}

