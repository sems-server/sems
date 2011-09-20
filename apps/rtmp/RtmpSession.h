#ifndef _RtmpSession_h_
#define _RtmpSession_h_

#include "Rtmp.h"
#include "AmSession.h"

#include "librtmp/rtmp.h"

class RtmpSessionEvent
  : public AmEvent
{
public:
  enum EvType {
    Disconnect,
    Accept
  };

  RtmpSessionEvent(EvType t)
    : AmEvent((int)t) {}

  EvType getEvType() { return (EvType)event_id; }
};

class RtmpAudio;
class RtmpConnection;

class RtmpSession 
  : public AmSession
{
  RtmpAudio*      rtmp_audio;

  RtmpConnection* rtmp_connection;
  AmMutex         m_rtmp_conn;

private:
  void sendCallState();
  void clearConnection();

public:
  RtmpSession(RtmpConnection* c);
  ~RtmpSession();

  // @see AmSession
  void onSessionStart();
  void onBye(const AmSipRequest& req);
  void onBeforeDestroy();
  void onSipReply(const AmSipReply& reply,
		  AmSipDialog::Status old_dlg_status);

  void onInvite(const AmSipRequest& req);

  // @see AmEventHandler
  void process(AmEvent*);

  // forwards the packet the RtmpAudio
  void bufferPacket(const RTMPPacket& p);

  // sets the connection pointer
  void setConnectionPtr(RtmpConnection* c);

  // sets the outgoing stream ID for RTMP audio packets
  void setPlayStreamID(unsigned int stream_id);

  // Sends the disconnect event to the session to terminate it
  void disconnect();

  // Sends the accept event to the session
  void accept();
};

#endif
