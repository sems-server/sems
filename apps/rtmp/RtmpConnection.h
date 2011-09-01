#ifndef _RtmpConnection_h_
#define _RtmpConnection_h_

#include "AmThread.h"
#include "librtmp/rtmp.h"

class RtmpSession;
class RtmpSender;
class RtmpAudio;

class RtmpConnection
  : public AmThread
{
  enum MediaFlags {
    AudioSend=0x01,
    AudioRecv=0x02,
    VideoSend=0x04,
    VideoRecv=0x08
  };

  RTMP     rtmp;

  int      streamID;
  int      arglen;
  int      argc;
  uint32_t filetime;	/* time of last download we started */
  AVal     filename;	/* name of last download */

  // Stream ID with play() invoke
  unsigned int play_stream_id;

  // Stream ID with publish() invoke
  unsigned int publish_stream_id;
  
  // Owned by the connection
  // used also by the session
  //
  // Note: do not destroy before
  //       the session released its ptrs.
  RtmpSender*  sender;

  // Owned by the session
  RtmpSession* session;
  AmMutex      m_session;

public:
  RtmpConnection(int fd);
  ~RtmpConnection();

  void setSessionPtr(RtmpSession* s);
  RtmpSender* getSenderPtr() { return sender; }

  int SendPlayStart();
  int SendPlayStop();
  int SendStreamBegin();
  int SendStreamEOF();

  int SendCallStatus(int status);

protected:
  void run();
  void on_stop();

private:
  RtmpSession* startSession(const char* uri);
  void disconnectSession();
  void detachSession();

  void stopStream(unsigned int stream_id);

  int processPacket(RTMPPacket *packet);
  int invoke(RTMPPacket *packet, unsigned int offset);

  void rxAudio(RTMPPacket *packet);
  void rxVideo(RTMPPacket *packet) {/*NYI*/}

  void HandleCtrl(const RTMPPacket *packet);

  int SendCtrl(short nType, unsigned int nObject, unsigned int nTime);
  int SendResultNumber(double txn, double ID);
  int SendConnectResult(double txn);
  int SendPause(int DoPause, int iTime);
  int SendChangeChunkSize();

  char* dumpAMF(AMFObject *obj, char *ptr, AVal *argv);
  int   countAMF(AMFObject *obj);
};

#endif
