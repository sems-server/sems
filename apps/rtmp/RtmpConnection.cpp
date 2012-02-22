/*
 * Copyright (C) 2011 Raphael Coeffic
 *
 * For the code parts originating from rtmpdump (rtmpsrv.c):
 *  Copyright (C) 2009 Andrej Stepanchuk
 *  Copyright (C) 2009 Howard Chu
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

#include <stdlib.h>
#include <string.h>

#include "RtmpConnection.h"
#include "RtmpSender.h"
#include "RtmpAudio.h"
#include "RtmpSession.h"
#include "RtmpUtils.h"

#include "AmSessionContainer.h"
#include "log.h"

#include "librtmp/log.h"

#include <fcntl.h>

/* interval we disallow duplicate requests, in msec */
#define DUPTIME       5000


RtmpConnection::RtmpConnection(int fd)
  : prev_stream_id(0),
    play_stream_id(0),
    publish_stream_id(0),
    sender(NULL),
    session(NULL),
    registered(false),
    di_reg_client(NULL),
    rtmp_cfg(NULL)
{
  memset(&rtmp,0,sizeof(RTMP));
  RTMP_Init(&rtmp);
  rtmp.m_sb.sb_socket = fd;

  ident = AmSession::getNewId();
  di_reg_client = RtmpFactory_impl::instance()->getRegClient();
  rtmp_cfg = RtmpFactory_impl::instance()->getConfig();
}

RtmpConnection::~RtmpConnection()
{
}

void RtmpConnection::setSessionPtr(RtmpSession* s)
{
  m_session.lock();
  DBG("session ptr = 0x%p\n",s);
  session = s;
  m_session.unlock();
}

void RtmpConnection::run()
{
  RTMPPacket packet;
  memset(&packet,0,sizeof(RTMPPacket));

  DBG("Starting connection (socket=%i)\n",rtmp.m_sb.sb_socket);

  if (!RTMP_Serve(&rtmp)) {
    ERROR("Handshake failed\n");
    RTMP_Close(&rtmp);
    return;
  }

  // from here on, we send asynchronously
  sender = new RtmpSender(&rtmp);
  if(!sender){
    ERROR("Could not allocate sender.\n");
    RTMP_Close(&rtmp);
    return;
  }
  sender->start();

  while(RTMP_IsConnected(&rtmp)) {

    if(!RTMP_ReadPacket(&rtmp,&packet)) {
      if(RTMP_IsTimedout(&rtmp))
	continue;
      else
	break;
    }

    if(!RTMPPacket_IsReady(&packet))
      continue;

    int err = processPacket(&packet);
    RTMPPacket_Free(&packet);

    if(err < 0) {
      ERROR("could not process packet\n");
      break;
    }
  }

  if(registered){
    RtmpFactory_impl::instance()->removeConnection(ident);
    removeRegistration();
    registered = false;
  }
  detachSession();

  // terminate the sender thread
  sender->stop();
  sender->join();
  delete sender;
  sender = NULL;

  RTMP_Close(&rtmp);
  DBG("connection closed\n");
  AmThreadWatcher::instance()->add(this);
}

void RtmpConnection::on_stop()
{}

int RtmpConnection::processPacket(RTMPPacket* packet)
{
  //DBG("received packet type %02X, size %u bytes, Stream ID %i",
  //    packet->m_packetType, packet->m_nBodySize, packet->m_nInfoField2);

  switch (packet->m_packetType) {
  case 0x01:
    // chunk size
    HandleChangeChunkSize(&rtmp, packet);
    break;

  case 0x03:
    // bytes read report
    break;
    
  case 0x04:
    // ctrl
    HandleCtrl(packet);
    break;

  case 0x05:
    // server bw
    DBG("Server BW msg not yet supported.\n");
    //TODO: HandleServerBW(&rtmp, packet);
    break;

  case 0x06:
    // client bw
    DBG("Client BW msg not yet supported.\n");
    //TODO: HandleClientBW(&rtmp, packet);
    break;

  case RTMP_PACKET_TYPE_AUDIO:
    // audio data
    //
    // note(rco): librtmp writes the absolute timestamp into every packet
    //            after parsing it.
    //
    //DBG("audio packet: ts = %8i\n",packet->m_nTimeStamp);
    rxAudio(packet);
    break;

  case RTMP_PACKET_TYPE_VIDEO:
    // video data
    DBG("video packet");
    rxVideo(packet);
    break;

  case 0x0F:			
    // flex stream send
    break;

  case 0x10:			
    // flex shared object
    break;

  case 0x11:			
    // flex message
    DBG("flex message\n");
    if (invoke(packet, 1))
      return -1;
    break;

  case RTMP_PACKET_TYPE_INFO:
    // metadata (notify)
    break;

  case 0x13:
    // shared object
    break;

  case INVOKE_PTYPE:
    // invoke
    DBG("invoke message\n");
    if (invoke(packet, 0))
      return -1;
    break;

  case 0x16:
    /* flv */
    break;

  default:
    DBG("unknown packet type (0x%02x)",packet->m_packetType);
    break;
  }

  return 0;
}

// Returns 0 for OK/Failed/error, 1 for 'Stop or Complete'
int
RtmpConnection::invoke(RTMPPacket *packet, unsigned int offset)
{
  const char *body;
  unsigned int nBodySize;
  int ret = 0, nRes;

  body = packet->m_body + offset;
  nBodySize = packet->m_nBodySize - offset;

  if (body[0] != 0x02)		// make sure it is a string method name we start with
    {
      WARN("Sanity failed. no string method in invoke packet");
      return 0;
    }

  AMFObject obj;
  nRes = AMF_Decode(&obj, body, nBodySize, FALSE);
  if (nRes < 0)
    {
      ERROR("error decoding invoke packet");
      return 0;
    }

  AMF_Dump(&obj);
  AVal method;
  AMFProp_GetString(AMF_GetProp(&obj, NULL, 0), &method);
  double txn = AMFProp_GetNumber(AMF_GetProp(&obj, NULL, 1));
  DBG("client invoking <%s>\n",method.av_val);

  if (AVMATCH(&method, &av_connect))
    {
      AMFObject cobj;
      AVal pname, pval;
      int i;

      packet->m_body = NULL;
      AMFProp_GetObject(AMF_GetProp(&obj, NULL, 2), &cobj);
      for (i=0; i<cobj.o_num; i++)
	{
	  pname = cobj.o_props[i].p_name;
	  pval.av_val = NULL;
	  pval.av_len = 0;
	  if (cobj.o_props[i].p_type == AMF_STRING)
	    pval = cobj.o_props[i].p_vu.p_aval;
	  if (AVMATCH(&pname, &av_app))
	    {
	      rtmp.Link.app = pval;
	      pval.av_val = NULL;
	      if (!rtmp.Link.app.av_val)
	        rtmp.Link.app.av_val = (char*)"";
	    }
	  else if (AVMATCH(&pname, &av_flashVer))
	    {
	      rtmp.Link.flashVer = pval;
	      pval.av_val = NULL;
	    }
	  else if (AVMATCH(&pname, &av_swfUrl))
	    {
	      rtmp.Link.swfUrl = pval;
	      pval.av_val = NULL;
	    }
	  else if (AVMATCH(&pname, &av_tcUrl))
	    {
	      rtmp.Link.tcUrl = pval;
	      pval.av_val = NULL;
	    }
	  else if (AVMATCH(&pname, &av_pageUrl))
	    {
	      rtmp.Link.pageUrl = pval;
	      pval.av_val = NULL;
	    }
	  else if (AVMATCH(&pname, &av_audioCodecs))
	    {
	      rtmp.m_fAudioCodecs = cobj.o_props[i].p_vu.p_number;
	    }
	  else if (AVMATCH(&pname, &av_videoCodecs))
	    {
	      rtmp.m_fVideoCodecs = cobj.o_props[i].p_vu.p_number;
	    }
	  else if (AVMATCH(&pname, &av_objectEncoding))
	    {
	      rtmp.m_fEncoding = cobj.o_props[i].p_vu.p_number;
	    }
	}
      sender->SendConnectResult(txn);
    }
  else if (AVMATCH(&method, &av_createStream))
    {
      sender->SendResultNumber(txn, ++prev_stream_id);
      DBG("createStream: new channel %i",prev_stream_id);
    }
  else if (AVMATCH(&method, &av_getStreamLength))
    {
      // TODO: find value for live streams (0?, -1?)
      sender->SendResultNumber(txn, 0.0);
    }
  else if (AVMATCH(&method, &av_play))
    {
      AMFProp_GetString(AMF_GetProp(&obj, NULL, 3), &rtmp.Link.playpath);

      DBG("playpath = <%.*s>\n",
	  rtmp.Link.playpath.av_len,
	  rtmp.Link.playpath.av_val);

      if (rtmp.Link.tcUrl.av_len)
	{

	  DBG("tcUrl = <%.*s>\n",
	      rtmp.Link.tcUrl.av_len,
	      rtmp.Link.tcUrl.av_val);

	  if (rtmp.Link.app.av_val) 
	    {
	      DBG("app = <%.*s>\n",
		  rtmp.Link.app.av_len,
		  rtmp.Link.app.av_val);
	    }

	  if (rtmp.Link.flashVer.av_val)
	    {
	      DBG("flashVer = <%.*s>\n",
		  rtmp.Link.flashVer.av_len,
		  rtmp.Link.flashVer.av_val);
	    }

	  if (rtmp.Link.swfUrl.av_val)
	    {
	      DBG("swfUrl = <%.*s>\n",
		  rtmp.Link.swfUrl.av_len,
		  rtmp.Link.swfUrl.av_val);
	    }

	  if (rtmp.Link.pageUrl.av_val)
	    {
	      DBG("pageUrl = <%.*s>\n",
		  rtmp.Link.pageUrl.av_len,
		  rtmp.Link.pageUrl.av_val);
	    }
	}
      
      play_stream_id = packet->m_nInfoField2;
      m_session.lock();
      if(session) {
	session->setPlayStreamID(play_stream_id);
	sender->SendStreamBegin();
	sender->SendPlayStart();
      }
      m_session.unlock();
    }
  else if(AVMATCH(&method, &av_publish))
    {
      DBG("Client is now publishing (Stream ID = %i)\n",
	  packet->m_nInfoField2);
      publish_stream_id = packet->m_nInfoField2;
    }
  else if(AVMATCH(&method, &av_closeStream))
    {
      DBG("received closeStream with StreamID=%i\n",packet->m_nInfoField2);
      stopStream(packet->m_nInfoField2);
    }
  else if(AVMATCH(&method, &av_deleteStream))
    {
      // - compare StreamID with play_stream_id
      //   - if matched: stop session
      unsigned int stream_id = (unsigned int)AMFProp_GetNumber(AMF_GetProp(&obj, NULL, 3));
      DBG("received deleteStream with StreamID=%i\n",stream_id);
      stopStream(stream_id);
    }
  else if(AVMATCH(&method, &av_dial))
    {
      AVal uri={0,0};

      if(obj.o_num > 3){
	AMFProp_GetString(AMF_GetProp(&obj, NULL, 3), &uri);
      }

      if(!uri.av_len){
	// missing URI parameter
	sender->SendErrorResult(txn,"Sono.Call.NoUri");
      }
      else {
	m_session.lock();

	if(session){
	  sender->SendErrorResult(txn,"Sono.Call.Existing");
	}
	else {
	  session = startSession(uri.av_val);
	  if(!session) {
	    sender->SendErrorResult(txn,"Sono.Call.Failed");
	  }
	}
	
	m_session.unlock();
      }
    }
  else if(AVMATCH(&method, &av_hangup))
    {
      disconnectSession();
    }
  else if(AVMATCH(&method, &av_register))
    {
      if(!registered){
	if(RtmpFactory_impl::instance()->addConnection(ident,this) < 0) {
	  ERROR("could not register RTMP connection (ident='%s')\n",ident.c_str());
	  sender->SendErrorResult(txn,"Sono.Registration.Failed");
	}
	else {
	  registered = true;
	  DBG("RTMP connection registered (ident='%s')\n",ident.c_str());
	  sender->SendRegisterResult(txn,ident.c_str());
	}
      }

      if(registered){
	if(di_reg_client &&
	   !rtmp_cfg->ImplicitRegistrar.empty()) {
	  createRegistration(rtmp_cfg->ImplicitRegistrar,
			     ident,rtmp_cfg->FromName);
	}
      }
    }
  else if(AVMATCH(&method, &av_accept))
    {
      m_session.lock();
      if(session) {
	session->accept();
      }
      else {
	//TODO: send errror
      }
      m_session.unlock();
    }

  AMF_Reset(&obj);
  return ret;
}

int RtmpConnection::SendCallStatus(int status)
{
  return sender->SendCallStatus(status);
}

int RtmpConnection::NotifyIncomingCall(const string& uri)
{
  return sender->NotifyIncomingCall(uri);
}

void RtmpConnection::HandleCtrl(const RTMPPacket *packet)
{
  short nType = -1;
  unsigned int tmp;
  if (packet->m_body && packet->m_nBodySize >= 2)
    nType = AMF_DecodeInt16(packet->m_body);
  DBG("received ctrl. type: %d, len: %d", 
      nType, packet->m_nBodySize);
  /*RTMP_LogHex(packet.m_body, packet.m_nBodySize);*/

  if (packet->m_nBodySize >= 6)
    {
      switch (nType)
	{
	case 0:
	  tmp = AMF_DecodeInt32(packet->m_body + 2);
	  DBG("Stream Begin %d", tmp);
	  break;

	case 1:
	  tmp = AMF_DecodeInt32(packet->m_body + 2);
	  DBG("Stream EOF %d", tmp);
	  if (rtmp.m_pausing == 1)
	    rtmp.m_pausing = 2;
	  break;

	case 2:
	  tmp = AMF_DecodeInt32(packet->m_body + 2);
	  DBG("Stream Dry %d", tmp);
	  break;

	case 3:
	  tmp = AMF_DecodeInt32(packet->m_body + 2);
	  DBG("Stream Ack %d", tmp);
	  break;

	case 4:
	  tmp = AMF_DecodeInt32(packet->m_body + 2);
	  DBG("Stream IsRecorded %d", tmp);
	  break;

	case 6:		/* server ping. reply with pong. */
	  tmp = AMF_DecodeInt32(packet->m_body + 2);
	  DBG("Ping %d", tmp);
	  sender->SendCtrl(0x07, tmp, 0);
	  break;

	/* FMS 3.5 servers send the following two controls to let the client
	 * know when the server has sent a complete buffer. I.e., when the
	 * server has sent an amount of data equal to m_nBufferMS in duration.
	 * The server meters its output so that data arrives at the client
	 * in realtime and no faster.
	 *
	 * The rtmpdump program tries to set m_nBufferMS as large as
	 * possible, to force the server to send data as fast as possible.
	 * In practice, the server appears to cap this at about 1 hour's
	 * worth of data. After the server has sent a complete buffer, and
	 * sends this BufferEmpty message, it will wait until the play
	 * duration of that buffer has passed before sending a new buffer.
	 * The BufferReady message will be sent when the new buffer starts.
	 * (There is no BufferReady message for the very first buffer;
	 * presumably the Stream Begin message is sufficient for that
	 * purpose.)
	 *
	 * If the network speed is much faster than the data bitrate, then
	 * there may be long delays between the end of one buffer and the
	 * start of the next.
	 *
	 * Since usually the network allows data to be sent at
	 * faster than realtime, and rtmpdump wants to download the data
	 * as fast as possible, we use this RTMP_LF_BUFX hack: when we
	 * get the BufferEmpty message, we send a Pause followed by an
	 * Unpause. This causes the server to send the next buffer immediately
	 * instead of waiting for the full duration to elapse. (That's
	 * also the purpose of the ToggleStream function, which rtmpdump
	 * calls if we get a read timeout.)
	 *
	 * Media player apps don't need this hack since they are just
	 * going to play the data in realtime anyway. It also doesn't work
	 * for live streams since they obviously can only be sent in
	 * realtime. And it's all moot if the network speed is actually
	 * slower than the media bitrate.
	 */
	case 31:
	  tmp = AMF_DecodeInt32(packet->m_body + 2);
	  RTMP_Log(RTMP_LOGDEBUG, "%s, Stream BufferEmpty %d", __FUNCTION__, tmp);
	  if (!(rtmp.Link.lFlags & RTMP_LF_BUFX))
	    break;
	  if (!rtmp.m_pausing)
	    {
	      rtmp.m_pauseStamp = rtmp.m_channelTimestamp[rtmp.m_mediaChannel];
	      sender->SendPause(TRUE, rtmp.m_pauseStamp);
	      rtmp.m_pausing = 1;
	    }
	  else if (rtmp.m_pausing == 2)
	    {
	      sender->SendPause(FALSE, rtmp.m_pauseStamp);
	      rtmp.m_pausing = 3;
	    }
	  break;

	case 32:
	  tmp = AMF_DecodeInt32(packet->m_body + 2);
	  RTMP_Log(RTMP_LOGDEBUG, "%s, Stream BufferReady %d", __FUNCTION__, tmp);
	  break;

	default:
	  tmp = AMF_DecodeInt32(packet->m_body + 2);
	  RTMP_Log(RTMP_LOGDEBUG, "%s, Stream xx %d", __FUNCTION__, tmp);
	  break;
	}

    }

  if (nType == 0x1A)
    {
      RTMP_Log(RTMP_LOGDEBUG, "%s, SWFVerification ping received: ", __FUNCTION__);
#ifdef CRYPTO
      /*RTMP_LogHex(packet.m_body, packet.m_nBodySize); */

      /* respond with HMAC SHA256 of decompressed SWF, key is the 30byte player key, also the last 30 bytes of the server handshake are applied */
      if (rtmp.Link.SWFSize)
	{
	  SendCtrl(0x1B, 0, 0);
	}
      else
	{
	  ERROR("Ignoring SWFVerification request, use --swfVfy!");
	}
#else
      ERROR("Ignoring SWFVerification request, no CRYPTO support!");
#endif
    }
}


//#define DUMP_AUDIO 1

#ifdef DUMP_AUDIO
static void dump_audio(RTMPPacket *packet)
{
  static int dump_fd=0;
  if(dump_fd == 0){
    dump_fd = open("speex_out.raw",O_WRONLY|O_CREAT|O_TRUNC,
		   S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
    if(dump_fd < 0)
      ERROR("could not open speex_out.raw: %s\n",strerror(errno));
  }
  if(dump_fd < 0) return;

  uint32_t pkg_size = packet->m_nBodySize-1;
  write(dump_fd,&pkg_size,sizeof(uint32_t));
  write(dump_fd,packet->m_body+1,pkg_size);
}
#endif

void RtmpConnection::rxAudio(RTMPPacket *packet)
{
  if(!packet) return;

#ifdef DUMP_AUDIO
  dump_audio();
#endif

// soundType 	(byte & 0x01) » 0 	
//   0: mono, 1: stereo
// soundSize 	(byte & 0x02) » 1 	
//   0: 8-bit, 1: 16-bit
// soundRate 	(byte & 0x0C) » 2 	
//   0: 5.5 kHz, 1: 11 kHz, 2: 22 kHz, 3: 44 kHz
// soundFormat 	(byte & 0xf0) » 4 	
//   0: Uncompressed, 1: ADPCM, 2: MP3, 5: Nellymoser 8kHz mono, 6: Nellymoser, 11: Speex 

  m_session.lock();
  // stream not yet started
  if(!session){
    m_session.unlock();
    return;
  }
  session->bufferPacket(*packet);
  m_session.unlock();
}

RtmpSession* RtmpConnection::startSession(const char* uri)
{
  auto_ptr<RtmpSession> n_session(new RtmpSession(this));
  AmSipDialog& dialout_dlg = n_session->dlg;

  string dialout_id = AmSession::getNewId();
  dialout_dlg.local_tag    = dialout_id;
  dialout_dlg.callid       = AmSession::getNewId();

  dialout_dlg.remote_party = "<" + string(uri) + ">";
  dialout_dlg.remote_uri   = uri;

  dialout_dlg.local_party  = "\"" + rtmp_cfg->FromName + "\" "
    "<sip:" + ident + "@";

  if(!rtmp_cfg->FromDomain.empty()){
    dialout_dlg.local_party += rtmp_cfg->FromDomain;
  }
  else {
    int out_if = dialout_dlg.getOutboundIf();
    dialout_dlg.local_party += AmConfig::Ifs[out_if].LocalSIPIP;
    if(AmConfig::Ifs[out_if].LocalSIPPort != 5060)
      dialout_dlg.local_party += ":" + int2str(AmConfig::Ifs[out_if].LocalSIPPort);
  }

  dialout_dlg.local_party += ">";
  
  n_session->setCallgroup(dialout_id);
  switch(AmSessionContainer::instance()->addSession(dialout_id,
						    n_session.get())){
  case AmSessionContainer::ShutDown:
    DBG("Server shuting down... do not create a new session.\n");
    return NULL;

  case AmSessionContainer::AlreadyExist:
    DBG("Session already exist !?\n");
    return NULL;

  case AmSessionContainer::Inserted:
    break;
  }
  
  AmMimeBody sdp_body;
  sdp_body.addPart(SIP_APPLICATION_SDP);

  RtmpSession* pn_session = n_session.release();
  if(dialout_dlg.sendRequest(SIP_METH_INVITE,&sdp_body) < 0) {
    ERROR("dialout_dlg.sendRequest() returned an error\n");
    AmSessionContainer::instance()->destroySession(pn_session);
    return NULL;
  }

  pn_session->start();
  return pn_session;
}

void RtmpConnection::detachSession()
{
  m_session.lock();
  DBG("detaching session: erasing session ptr... (s=%p)\n",session);

  if(session){
    session->setConnectionPtr(NULL);
    session = NULL;
  }
  m_session.unlock();
}

void RtmpConnection::disconnectSession()
{
  m_session.lock();
  if(session){
    session->disconnect();
  }
  m_session.unlock();
}

void RtmpConnection::createRegistration(const string& domain,
					const string& user,
					const string& display_name,
					const string& auth_user,
					const string& passwd)
{
  if(!di_reg_client) return;

  AmArg di_args,ret;
  di_args.push(domain.c_str());
  di_args.push(user.c_str());
  di_args.push(display_name.c_str());  // display name
  di_args.push(auth_user.c_str());     // auth_user
  di_args.push(passwd.c_str());        // pwd
  di_args.push(FACTORY_Q_NAME);
  di_args.push("");
  di_args.push("");
  di_args.push(ident.c_str());
  
  di_reg_client->invoke("createRegistration", di_args, ret);
}

void RtmpConnection::removeRegistration()
{
  if(!di_reg_client || ident.empty()) return;
  AmArg di_args,ret;
  di_args.push(ident.c_str());
  di_reg_client->invoke("removeRegistration", di_args, ret);
}

void RtmpConnection::stopStream(unsigned int stream_id)
{
  if(stream_id == play_stream_id){
    play_stream_id = 0;
    sender->SendStreamEOF();
    sender->SendPlayStop();
  }
  else if(stream_id == publish_stream_id) {
    publish_stream_id = 0;
  }
}
