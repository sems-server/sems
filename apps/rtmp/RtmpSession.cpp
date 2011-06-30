#include "RtmpSession.h"
#include "RtmpAudio.h"
#include "RtmpConnection.h"

RtmpSession::RtmpSession(RtmpConnection* c, unsigned int stream_id)
  : AmSession(), 
    rtmp_audio(new RtmpAudio(c->getSenderPtr(),
			     stream_id)),
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
    rtmp_connection->SendStreamEOF();
    rtmp_connection->SendPlayStop();
    rtmp_connection->setSessionPtr(NULL);
    rtmp_connection = NULL;
  }
  m_rtmp_conn.unlock();

  AmSession::onBeforeDestroy();
}

void RtmpSession::onSessionStart()
{
  DBG("enabling adaptive buffer\n");
  RTPStream()->setPlayoutType(ADAPTIVE_PLAYOUT);
  DBG("plugging rtmp_audio into in&out\n");
  setInOut(rtmp_audio,rtmp_audio);
}

void RtmpSession::onBye(const AmSipRequest& req)
{
  DBG("onBye(...)\n");
  AmSession::onBye(req);
}

void RtmpSession::bufferPacket(const RTMPPacket& p)
{
  rtmp_audio->bufferPacket(p);
}

void RtmpSession::setConnectionPtr(RtmpConnection* c)
{
  m_rtmp_conn.lock();
  DBG("resetting sender ptr used by rtmp_audio (c=%p)\n",c);
  if(c){
    rtmp_audio->setSenderPtr(c->getSenderPtr());
  }
  else {
    clearAudio();
    rtmp_audio->setSenderPtr(NULL);
    //TODO: terminate the session
    //warning: this is not executed from event handler threads!!!
  }
  rtmp_connection = c;
  m_rtmp_conn.unlock();
}

void RtmpSession::onAudioEvent(AmAudioEvent* audio_ev)
{
  if (audio_ev->event_id == AmAudioEvent::cleared)
    dlg.bye();
  
  AmSession::onAudioEvent(audio_ev);
}
