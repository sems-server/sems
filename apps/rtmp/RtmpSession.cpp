#include "RtmpSession.h"
#include "RtmpAudio.h"
#include "RtmpConnection.h"

// call states for the RTMP client
#define RTMP_CALL_NOT_CONNECTED 0
#define RTMP_CALL_IN_PROGRESS   1
#define RTMP_CALL_CONNECTED     2
#define RTMP_CALL_DISCONNECTING 3

// request the client to connect the streams
#define RTMP_CALL_CONNECT_STREAMS 4

const unsigned int __dlg_status2rtmp_call[AmSipDialog::__max_Status]  = {
  RTMP_CALL_NOT_CONNECTED, // Disconnected
  RTMP_CALL_IN_PROGRESS, //"Trying",
  RTMP_CALL_IN_PROGRESS, //"Proceeding",
  RTMP_CALL_DISCONNECTING, //"Cancelling",
  RTMP_CALL_IN_PROGRESS, //"Early",
  RTMP_CALL_CONNECTED, //"Connected",
  RTMP_CALL_DISCONNECTING //"Disconnecting"
};

RtmpSession::RtmpSession(RtmpConnection* c)
  : AmSession(), 
    rtmp_audio(new RtmpAudio(c->getSenderPtr())),
    rtmp_connection(c)
{
}

RtmpSession::~RtmpSession()
{
  delete rtmp_audio;
}

void RtmpSession::onBeforeDestroy()
{
  m_rtmp_conn.lock();
  if(rtmp_connection){
    rtmp_connection->setSessionPtr(NULL);
    rtmp_connection = NULL;
  }
  m_rtmp_conn.unlock();

  AmSession::onBeforeDestroy();
}

void RtmpSession::onSessionStart()
{
  bool start_session = true;

  m_rtmp_conn.lock();
  if(rtmp_connection)
    rtmp_connection->SendCallStatus(RTMP_CALL_CONNECT_STREAMS);
  else
    start_session = false;
  m_rtmp_conn.unlock();

  if(!start_session) {
    setStopped();
    return;
  }

  DBG("enabling adaptive buffer\n");
  RTPStream()->setPlayoutType(ADAPTIVE_PLAYOUT);
  DBG("plugging rtmp_audio into in&out\n");
  setInOut(rtmp_audio,rtmp_audio);
  
  AmSession::onSessionStart();
}

void RtmpSession::onBye(const AmSipRequest& req)
{
  DBG("onBye(...)\n");
  AmSession::onBye(req);
}

void RtmpSession::onSipReply(const AmSipReply& reply,
			     AmSipDialog::Status old_dlg_status)
{
  m_rtmp_conn.lock();
  if(rtmp_connection){
    DBG("Dialog status: %s\n",dlg.getStatusStr());
    unsigned int rtmp_call_status = __dlg_status2rtmp_call[dlg.getStatus()];
    rtmp_connection->SendCallStatus(rtmp_call_status);
  }
  m_rtmp_conn.unlock();

  AmSession::onSipReply(reply,old_dlg_status);
}

void RtmpSession::process(AmEvent* ev)
{
  RtmpSessionEvent* rtmp_ev = dynamic_cast<RtmpSessionEvent*>(ev);
  if(rtmp_ev){
    dlg.bye();
    setStopped();
    return;
  }

  AmSession::process(ev);
}

void RtmpSession::bufferPacket(const RTMPPacket& p)
{
  rtmp_audio->bufferPacket(p);
}

void RtmpSession::setConnectionPtr(RtmpConnection* c)
{
  //warning: this is not executed from event handler threads!!!
  m_rtmp_conn.lock();
  DBG("resetting sender ptr used by rtmp_audio (c=%p)\n",c);
  if(c){
    rtmp_audio->setSenderPtr(c->getSenderPtr());
  }
  else {
    rtmp_audio->setSenderPtr(NULL);
    disconnect();
  }
  rtmp_connection = c;
  m_rtmp_conn.unlock();
}

// sets the outgoing stream ID for RTMP audio packets
void  RtmpSession::setPlayStreamID(unsigned int stream_id)
{
  rtmp_audio->setPlayStreamID(stream_id);
}

void RtmpSession::disconnect()
{
  postEvent(new RtmpSessionEvent(RtmpSessionEvent::Disconnect));
}
