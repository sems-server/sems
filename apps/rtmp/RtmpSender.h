#ifndef _RtmpSender_h_
#define _RtmpSender_h_

#include "AmThread.h"
#include <queue>
using std::queue;

#include "librtmp/rtmp.h"

class RtmpSender
  : public AmThread
{
  // sender queue
  queue<RTMPPacket> q_send;
  AmMutex           m_q_send;
  AmCondition<bool> has_work;

  // ptr to RtmpConnection::rtmp
  RTMP* p_rtmp;

  // execution control
  AmSharedVar<bool> running;

  int SendChangeChunkSize();

protected:
  void run();
  void on_stop();

public:
  RtmpSender(RTMP* r);
  ~RtmpSender();

  // adds a packet to the sender queue
  int push_back(const RTMPPacket& p);
};

#endif
