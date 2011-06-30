#include <stdlib.h>
#include <string.h>

#include "RtmpConnection.h"
#include "RtmpSender.h"
#include "RtmpAudio.h"
#include "RtmpSession.h"

#include "AmSessionContainer.h"
#include "log.h"

#include "librtmp/log.h"

#include <fcntl.h>

/* interval we disallow duplicate requests, in msec */
#define DUPTIME       5000

#define INVOKE_PTYPE    0x14

#define CONTROL_CHANNEL 0x03

#define _AVC(s) {(char*)s,sizeof(s)-1}
#define SAVC(x) static const AVal av_##x = {(char*)#x,sizeof(#x)-1}
#define STR2AVAL(av,str) \
   { \
     av.av_val = (char*)str; \
     av.av_len = strlen(av.av_val); \
   }

SAVC(app);
SAVC(connect);
SAVC(flashVer);
SAVC(swfUrl);
SAVC(pageUrl);
SAVC(tcUrl);
SAVC(fpad);
SAVC(capabilities);
SAVC(audioCodecs);
SAVC(videoCodecs);
SAVC(videoFunction);
SAVC(objectEncoding);
SAVC(_result);
SAVC(createStream);
SAVC(closeStream);
SAVC(deleteStream);
SAVC(getStreamLength);
SAVC(play);
SAVC(fmsVer);
SAVC(mode);
SAVC(level);
SAVC(code);
SAVC(description);
SAVC(secureToken);
SAVC(publish);

RtmpConnection::RtmpConnection(int fd)
  : sock_fd(fd), streamID(0),
    arglen(0), argc(0),
    filetime(0), /* time of last download we started */
    filename(), /* name of last download */
    play_stream_id(0),
    publish_stream_id(0),
    sender(NULL),
    session(NULL)
{}

RtmpConnection::~RtmpConnection()
{
  if(sock_fd > 0){
    ::close(sock_fd);
    sock_fd = 0;
  }
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

  memset(&rtmp,0,sizeof(RTMP));
  memset(&packet,0,sizeof(RTMPPacket));

  DBG("Starting connection (%zd;%zd)\n",
      sizeof(RTMP),sizeof(RTMPPacket));

  RTMP_Init(&rtmp);
  rtmp.m_sb.sb_socket = sock_fd;

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
	      arglen += 6 + pval.av_len;
	      argc += 2;
	    }
	  else if (AVMATCH(&pname, &av_flashVer))
	    {
	      rtmp.Link.flashVer = pval;
	      pval.av_val = NULL;
	      arglen += 6 + pval.av_len;
	      argc += 2;
	    }
	  else if (AVMATCH(&pname, &av_swfUrl))
	    {
	      rtmp.Link.swfUrl = pval;
	      pval.av_val = NULL;
	      arglen += 6 + pval.av_len;
	      argc += 2;
	    }
	  else if (AVMATCH(&pname, &av_tcUrl))
	    {
	      rtmp.Link.tcUrl = pval;
	      pval.av_val = NULL;
	      arglen += 6 + pval.av_len;
	      argc += 2;
	    }
	  else if (AVMATCH(&pname, &av_pageUrl))
	    {
	      rtmp.Link.pageUrl = pval;
	      pval.av_val = NULL;
	      arglen += 6 + pval.av_len;
	      argc += 2;
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
      /* Still have more parameters? Copy them */
      if (obj.o_num > 3)
	{
	  int i = obj.o_num - 3;
	  rtmp.Link.extras.o_num = i;
	  rtmp.Link.extras.o_props = (AMFObjectProperty*)malloc(i*sizeof(AMFObjectProperty));
	  memcpy(rtmp.Link.extras.o_props, obj.o_props+3, i*sizeof(AMFObjectProperty));
	  obj.o_num = 3;
	  arglen += countAMF(&rtmp.Link.extras);
	}
      SendConnectResult(txn);
    }
  else if (AVMATCH(&method, &av_createStream))
    {
      SendResultNumber(txn, ++streamID);
      DBG("createStream: new channel %i",streamID);
    }
  else if (AVMATCH(&method, &av_getStreamLength))
    {
      // TODO: find value for live streams (0?, -1?)
      SendResultNumber(txn, 0.0);
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

	  if (rtmp.Link.extras.o_num) {
	    //DBG("parsing extras\n");
	    //ptr = dumpAMF(&rtmp.Link.extras, ptr, argv);
	    AMF_Reset(&rtmp.Link.extras);
	  }
	}
      
      string uri(rtmp.Link.playpath.av_val,rtmp.Link.playpath.av_len);
      play_stream_id = packet->m_nInfoField2;
      startSession(uri.c_str(),play_stream_id);
    }
  else if(AVMATCH(&method, &av_publish))
    {
      DBG("Client is now publishing (Stream ID = %i)\n",
	  packet->m_nInfoField2);
      publish_stream_id = packet->m_nInfoField2;
    }
  else if(AVMATCH(&method, &av_closeStream))
    {
      // - compare StreamID with play_stream_id
      //   - if matched: stop session
      DBG("received closeStream with StreamID=%i\n",packet->m_nInfoField2);
      if((packet->m_nInfoField2 == (int)play_stream_id) ||
	 packet->m_nInfoField2 == (int)publish_stream_id) {
	DBG("closeStream corresponds to [play|publish]_stream_id = %i\n",packet->m_nInfoField2);
	detachSession();
	play_stream_id = 0;
	publish_stream_id = 0;
      }
    }
  else if(AVMATCH(&method, &av_deleteStream))
    {
      //TODO: find out what to do here...
      // - fetch streamID
      // - compare with play_stream_id
      //   - if matched: stop session
    }

  AMF_Reset(&obj);
  return ret;
}

int RtmpConnection::SendConnectResult(double txn)
{
  RTMPPacket packet;
  char pbuf[384], *pend = pbuf+sizeof(pbuf);
  AMFObject obj;
  AMFObjectProperty p, op;
  AVal av;

  packet.m_nChannel   = CONTROL_CHANNEL;
  packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
  packet.m_packetType = INVOKE_PTYPE;
  packet.m_nTimeStamp = 0;
  packet.m_nInfoField2 = 0;
  packet.m_hasAbsTimestamp = 0;
  packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

  char *enc = packet.m_body;
  enc = AMF_EncodeString(enc, pend, &av__result);
  enc = AMF_EncodeNumber(enc, pend, txn);
  *enc++ = AMF_OBJECT;

  STR2AVAL(av, "FMS/3,5,1,525");
  enc = AMF_EncodeNamedString(enc, pend, &av_fmsVer, &av);
  enc = AMF_EncodeNamedNumber(enc, pend, &av_capabilities, 31.0);
  enc = AMF_EncodeNamedNumber(enc, pend, &av_mode, 1.0);
  *enc++ = 0;
  *enc++ = 0;
  *enc++ = AMF_OBJECT_END;

  *enc++ = AMF_OBJECT;

  STR2AVAL(av, "status");
  enc = AMF_EncodeNamedString(enc, pend, &av_level, &av);
  STR2AVAL(av, "NetConnection.Connect.Success");
  enc = AMF_EncodeNamedString(enc, pend, &av_code, &av);
  STR2AVAL(av, "Connection succeeded.");
  enc = AMF_EncodeNamedString(enc, pend, &av_description, &av);
  enc = AMF_EncodeNamedNumber(enc, pend, &av_objectEncoding, rtmp.m_fEncoding);
  STR2AVAL(p.p_name, "version");
  STR2AVAL(p.p_vu.p_aval, "3,5,1,525");
  p.p_type = AMF_STRING;
  obj.o_num = 1;
  obj.o_props = &p;
  op.p_type = AMF_OBJECT;
  STR2AVAL(op.p_name, "data");
  op.p_vu.p_object = obj;
  enc = AMFProp_Encode(&op, enc, pend);
  *enc++ = 0;
  *enc++ = 0;
  *enc++ = AMF_OBJECT_END;
  *enc++ = 0;
  *enc++ = 0;
  *enc++ = AMF_OBJECT_END;

  packet.m_nBodySize = enc - packet.m_body;

  return sender->push_back(packet);
}

int RtmpConnection::SendResultNumber(double txn, double ID)
{
  RTMPPacket packet;
  char pbuf[256], *pend = pbuf+sizeof(pbuf);

  packet.m_nChannel   = CONTROL_CHANNEL;
  packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
  packet.m_packetType = INVOKE_PTYPE;
  packet.m_nTimeStamp = 0;
  packet.m_nInfoField2 = 0;
  packet.m_hasAbsTimestamp = 0;
  packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

  char *enc = packet.m_body;
  enc = AMF_EncodeString(enc, pend, &av__result);
  enc = AMF_EncodeNumber(enc, pend, txn);
  *enc++ = AMF_NULL;
  enc = AMF_EncodeNumber(enc, pend, ID);

  packet.m_nBodySize = enc - packet.m_body;

  return sender->push_back(packet);
}

int RtmpConnection::countAMF(AMFObject *obj)
{
  int i, len;

  for (i=0, len=0; i < obj->o_num; i++)
    {
      AMFObjectProperty *p = &obj->o_props[i];
      len += 4;
      argc+= 2;
      if (p->p_name.av_val)
	len += 1;
      len += 2;
      if (p->p_name.av_val)
	len += p->p_name.av_len + 1;
      switch(p->p_type)
	{
	case AMF_BOOLEAN:
	  len += 1;
	  break;
	case AMF_STRING:
	  len += p->p_vu.p_aval.av_len;
	  break;
	case AMF_NUMBER:
	  len += 40;
	  break;
	case AMF_OBJECT:
	  len += 9;
	  len += countAMF(&p->p_vu.p_object);
	  argc += 2;
	  break;
	case AMF_NULL:
	default:
	  break;
	}
    }
  return len;
}

char* RtmpConnection::dumpAMF(AMFObject *obj, char *ptr, AVal *argv)
{
  int i, len, ac = argc;
  const char opt[] = "NBSO Z";

  for (i=0, len=0; i < obj->o_num; i++)
    {
      AMFObjectProperty *p = &obj->o_props[i];
      argv[ac].av_val = ptr+1;
      argv[ac++].av_len = 2;
      ptr += sprintf(ptr, " -C ");
      argv[ac].av_val = ptr;
      if (p->p_name.av_val)
	*ptr++ = 'N';
      *ptr++ = opt[p->p_type];
      *ptr++ = ':';
      if (p->p_name.av_val)
	ptr += sprintf(ptr, "%.*s:", p->p_name.av_len, p->p_name.av_val);
      switch(p->p_type)
	{
	case AMF_BOOLEAN:
	  *ptr++ = p->p_vu.p_number != 0 ? '1' : '0';
	  argv[ac].av_len = ptr - argv[ac].av_val;
	  break;
	case AMF_STRING:
	  memcpy(ptr, p->p_vu.p_aval.av_val, p->p_vu.p_aval.av_len);
	  ptr += p->p_vu.p_aval.av_len;
	  argv[ac].av_len = ptr - argv[ac].av_val;
	  break;
	case AMF_NUMBER:
	  ptr += sprintf(ptr, "%f", p->p_vu.p_number);
	  argv[ac].av_len = ptr - argv[ac].av_val;
	  break;
	case AMF_OBJECT:
	  *ptr++ = '1';
	  argv[ac].av_len = ptr - argv[ac].av_val;
	  ac++;
	  argc = ac;
	  ptr = dumpAMF(&p->p_vu.p_object, ptr, argv);
	  ac = argc;
	  argv[ac].av_val = ptr+1;
	  argv[ac++].av_len = 2;
	  argv[ac].av_val = ptr+4;
	  argv[ac].av_len = 3;
	  ptr += sprintf(ptr, " -C O:0");
	  break;
	case AMF_NULL:
	default:
	  argv[ac].av_len = ptr - argv[ac].av_val;
	  break;
	}
      ac++;
    }
  argc = ac;
  return ptr;
}

SAVC(onStatus);
SAVC(status);
static const AVal av_NetStream_Play_Start = _AVC("NetStream.Play.Start");
static const AVal av_Started_playing = _AVC("Started playing");
static const AVal av_NetStream_Play_Stop = _AVC("NetStream.Play.Stop");
static const AVal av_Stopped_playing = _AVC("Stopped playing");
SAVC(details);
SAVC(clientid);
SAVC(pause);

int RtmpConnection::SendPlayStart()
{
  RTMPPacket packet;
  char pbuf[384], *pend = pbuf+sizeof(pbuf);

  packet.m_nChannel   = CONTROL_CHANNEL;
  packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
  packet.m_packetType = INVOKE_PTYPE;
  packet.m_nTimeStamp = 0;
  packet.m_nInfoField2 = 0;
  packet.m_hasAbsTimestamp = 0;
  packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

  char *enc = packet.m_body;
  enc = AMF_EncodeString(enc, pend, &av_onStatus);
  enc = AMF_EncodeNumber(enc, pend, 0);

  *enc++ = AMF_NULL;//rco: seems to be needed
  *enc++ = AMF_OBJECT;

  enc = AMF_EncodeNamedString(enc, pend, &av_level, &av_status);
  enc = AMF_EncodeNamedString(enc, pend, &av_code, &av_NetStream_Play_Start);
  enc = AMF_EncodeNamedString(enc, pend, &av_description, &av_Started_playing);
  enc = AMF_EncodeNamedString(enc, pend, &av_details, &rtmp.Link.playpath);
  enc = AMF_EncodeNamedString(enc, pend, &av_clientid, &av_clientid);
  *enc++ = 0;
  *enc++ = 0;
  *enc++ = AMF_OBJECT_END;

  packet.m_nBodySize = enc - packet.m_body;
  return sender->push_back(packet);
}

int RtmpConnection::SendPlayStop()
{
  RTMPPacket packet;
  char pbuf[384], *pend = pbuf+sizeof(pbuf);

  packet.m_nChannel   = CONTROL_CHANNEL;
  packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
  packet.m_packetType = INVOKE_PTYPE;
  packet.m_nTimeStamp = 0;
  packet.m_nInfoField2 = 0;
  packet.m_hasAbsTimestamp = 0;
  packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

  char *enc = packet.m_body;
  enc = AMF_EncodeString(enc, pend, &av_onStatus);
  enc = AMF_EncodeNumber(enc, pend, 0);

  *enc++ = AMF_NULL;//rco: needed!
  *enc++ = AMF_OBJECT;

  enc = AMF_EncodeNamedString(enc, pend, &av_level, &av_status);
  enc = AMF_EncodeNamedString(enc, pend, &av_code, &av_NetStream_Play_Stop);
  enc = AMF_EncodeNamedString(enc, pend, &av_description, &av_Stopped_playing);
  enc = AMF_EncodeNamedString(enc, pend, &av_details, &rtmp.Link.playpath);
  enc = AMF_EncodeNamedString(enc, pend, &av_clientid, &av_clientid);
  *enc++ = 0;
  *enc++ = 0;
  *enc++ = AMF_OBJECT_END;

  packet.m_nBodySize = enc - packet.m_body;
  return sender->push_back(packet);
}

int RtmpConnection::SendStreamBegin()
{
  return SendCtrl(0, 1, 0);
}

int RtmpConnection::SendStreamEOF()
{
  return SendCtrl(1, 1, 0);
}

int RtmpConnection::SendPause(int DoPause, int iTime)
{
  RTMPPacket packet;
  char pbuf[256], *pend = pbuf + sizeof(pbuf);
  char *enc;

  packet.m_nChannel = 0x08;	/* video channel */
  packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
  packet.m_packetType = 0x14;	/* invoke */
  packet.m_nTimeStamp = 0;
  packet.m_nInfoField2 = 0;
  packet.m_hasAbsTimestamp = 0;
  packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

  enc = packet.m_body;
  enc = AMF_EncodeString(enc, pend, &av_pause);
  enc = AMF_EncodeNumber(enc, pend, ++rtmp.m_numInvokes);
  *enc++ = AMF_NULL;
  enc = AMF_EncodeBoolean(enc, pend, DoPause);
  enc = AMF_EncodeNumber(enc, pend, (double)iTime);

  packet.m_nBodySize = enc - packet.m_body;

  DBG("%d, pauseTime=%d", DoPause, iTime);
  return sender->push_back(packet);
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
	  SendCtrl(0x07, tmp, 0);
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
	      SendPause(TRUE, rtmp.m_pauseStamp);
	      rtmp.m_pausing = 1;
	    }
	  else if (rtmp.m_pausing == 2)
	    {
	      SendPause(FALSE, rtmp.m_pauseStamp);
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

/*
from http://jira.red5.org/confluence/display/docs/Ping:

Ping is the most mysterious message in RTMP and till now we haven't fully interpreted it yet. In summary, Ping message is used as a special command that are exchanged between client and server. This page aims to document all known Ping messages. Expect the list to grow.

The type of Ping packet is 0x4 and contains two mandatory parameters and two optional parameters. The first parameter is the type of Ping and in short integer. The second parameter is the target of the ping. As Ping is always sent in Channel 2 (control channel) and the target object in RTMP header is always 0 which means the Connection object, it's necessary to put an extra parameter to indicate the exact target object the Ping is sent to. The second parameter takes this responsibility. The value has the same meaning as the target object field in RTMP header. (The second value could also be used as other purposes, like RTT Ping/Pong. It is used as the timestamp.) The third and fourth parameters are optional and could be looked upon as the parameter of the Ping packet. Below is an unexhausted list of Ping messages.

    * type 0: Clear the stream. No third and fourth parameters. The second parameter could be 0. After the connection is established, a Ping 0,0 will be sent from server to client. The message will also be sent to client on the start of Play and in response of a Seek or Pause/Resume request. This Ping tells client to re-calibrate the clock with the timestamp of the next packet server sends.
    * type 1: Tell the stream to clear the playing buffer.
    * type 3: Buffer time of the client. The third parameter is the buffer time in millisecond.
    * type 4: Reset a stream. Used together with type 0 in the case of VOD. Often sent before type 0.
    * type 6: Ping the client from server. The second parameter is the current time.
    * type 7: Pong reply from client. The second parameter is the time the server sent with his ping request.
    * type 26: SWFVerification request
    * type 27: SWFVerification response
*/
int
RtmpConnection::SendCtrl(short nType, unsigned int nObject, unsigned int nTime)
{
  RTMPPacket packet;
  char pbuf[256], *pend = pbuf + sizeof(pbuf);
  int nSize;
  char *buf;

  DBG("sending ctrl. type: 0x%04x", (unsigned short)nType);

  packet.m_nChannel = 0x02;	/* control channel (ping) */
  packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
  packet.m_packetType = 0x04;	/* ctrl */
  packet.m_nTimeStamp = 0;	/* RTMP_GetTime(); */
  packet.m_nInfoField2 = 0;
  packet.m_hasAbsTimestamp = 0;
  packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

  switch(nType) {
  case 0x03: nSize = 10; break;	/* buffer time */
  case 0x1A: nSize = 3; break;	/* SWF verify request */
  case 0x1B: nSize = 44; break;	/* SWF verify response */
  default: nSize = 6; break;
  }

  packet.m_nBodySize = nSize;

  buf = packet.m_body;
  buf = AMF_EncodeInt16(buf, pend, nType);

  if (nType == 0x1B)
    {
#ifdef CRYPTO
      memcpy(buf, rtmp.Link.SWFVerificationResponse, 42);
      DBG("Sending SWFVerification response: ");
      RTMP_LogHex(RTMP_LOGDEBUG, (uint8_t *)packet.m_body, packet.m_nBodySize);
#endif
    }
  else if (nType == 0x1A)
    {
	  *buf = nObject & 0xff;
	}
  else
    {
      if (nSize > 2)
	buf = AMF_EncodeInt32(buf, pend, nObject);

      if (nSize > 6)
	buf = AMF_EncodeInt32(buf, pend, nTime);
    }

  return sender->push_back(packet);
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

RtmpSession* RtmpConnection::startSession(const char* uri, unsigned int stream_id)
{
  m_session.lock();

  if(session){
    WARN("session already running: doing nothing!\n");
    return NULL;
  }

  string dialout_id = AmSession::getNewId();

  auto_ptr<RtmpSession> n_session(new RtmpSession(this,stream_id));
  AmSipDialog& dialout_dlg = n_session->dlg;

  dialout_dlg.local_tag    = dialout_id;
  dialout_dlg.callid       = AmSession::getNewId();
  dialout_dlg.local_party  = "\"RTMP Gateway\" <sip:sems@mediaslug>";
  dialout_dlg.remote_party = "<" + string(uri) + ">";
  dialout_dlg.remote_uri   = uri;

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

  RtmpSession* pn_session = n_session.release();
  if(dialout_dlg.sendRequest(SIP_METH_INVITE,"application/sdp") < 0) {
    ERROR("dialout_dlg.sendRequest() returned an error\n");
    AmSessionContainer::instance()->destroySession(pn_session);
    return NULL;
  }

  if(pn_session){
    SendStreamBegin();
    SendPlayStart();
  }
  else {
    SendStreamEOF();
    SendPlayStop();
  }

  pn_session->start();
  session = pn_session;
  m_session.unlock();

  return pn_session;
}

void RtmpConnection::detachSession()
{
  m_session.lock();
  DBG("detaching session: erasing session ptr... (s=%p)\n",session);

  if(session){
    //TODO: terminate session
    session->setConnectionPtr(NULL);
    session = NULL;
  }
  m_session.unlock();
}
