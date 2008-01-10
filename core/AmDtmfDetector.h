/*
 * $Id$
 *
 * Copyright (C) 2005 Andriy I Pylypenko
 *
 * This file is part of sems, a free SIP media server.
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
/** @file AmDtmfDetector.h */
#ifndef _AmDtmfDetector_h_
#define _AmDtmfDetector_h_

#include "AmEventQueue.h"
#include "rtp/telephone_event.h"

#include <string>
#include <memory>
using namespace std;

#ifdef USE_SPANDSP
#include <math.h>
#include "spandsp.h"
#endif


//
// Forward declarations
//
class AmSession;
class AmDtmfDetector;
class AmDtmfEventHandler;
class AmRequest;

namespace Dtmf
{
  enum EventSource { SOURCE_RTP, SOURCE_SIP, SOURCE_INBAND, SOURCE_DETECTOR };

  enum InbandDetectorType { SEMSInternal, SpanDSP }; 
};
/**
 * \brief sink for audio to be processed by the inband DTMF detector 
 * 
 * This class adds processing of timeouts for DTMF detection
 */
class AmDtmfEventQueue : public AmEventQueue
{
 private:
  AmDtmfDetector *m_detector;

 public:
  AmDtmfEventQueue(AmDtmfDetector *);
  /**
   * Reimplemented abstract method from AmEventQueue
   */
  void processEvents();
  void putDtmfAudio(const unsigned char *, int size, int user_ts);
};

/**
 * \brief Base class for DTMF events
 */
class AmDtmfEvent : public AmEvent
{
 protected:
  /**
   * Code of the key
   */
  int m_event;
  /**
   * Duration of keypress in miliseconds
   */
  int m_duration_msec;

  /**
   * Constructor
   */
  AmDtmfEvent(int id)
    : AmEvent(id)
    {
    }

 public:
  AmDtmfEvent(int event, int duration)
    : AmEvent(Dtmf::SOURCE_DETECTOR), m_event(event), m_duration_msec(duration)
    {
    }
  /**
   * Code of the key
   */
  int event() const { return m_event; }
  /**
   * Duration of keypress in miliseconds
   */
  int duration() const { return m_duration_msec; }
};
/**
 * \brief Interface for a sink for KeyPresses.
 */
class AmKeyPressSink {
 public:
  AmKeyPressSink() { }
  virtual ~AmKeyPressSink() { }

  /**
   * Through this method the AmDtmfDetector receives events that was
   * detected by specific detectors.
   * @param event code of key pressed
   * @param source which detector posted this event
   * @param start time when key was pressed
   * @param stop time when key was released
   */
  virtual void registerKeyReleased(int event, Dtmf::EventSource source,
				   const struct timeval& start, const struct timeval& stop) = 0;
  /**
   * Through this method the AmDtmfDetector receives events that was
   * detected by specific detectors.
   * @param event code of key released
   * @param source which detector posted this event
   */
  virtual void registerKeyPressed(int event, Dtmf::EventSource source) = 0;
};

/**
 * \brief DTMF received via RTP
 */
class AmRtpDtmfEvent : public AmDtmfEvent
{
 private:
  /**
   * E flag from RTP packet
   */
  int m_e;
  /**
   * Volume value from RTP packet
   */
  int m_volume;

 public:
  /**
   * Constructor
   * @param payload data from rtp packet of payload type telephone-event
   * @param sample_rate sampling rate (from SDP payload description)
   */
  AmRtpDtmfEvent(const dtmf_payload_t *payload, int sample_rate);

  /**
   * Volume value from RTP packet
   */
  int volume() { return m_volume; }
  /**
   * E flag from RTP packet
   */
  int e() { return m_e; }
};

/**
 * \brief DTMF received via SIP INFO request
 */
class AmSipDtmfEvent : public AmDtmfEvent
{
 private:
  /**
   * Parser for application/dtmf-relay
   */
  void parseRequestBody(const string&);
  /**
   * Parser for application/dtmf-relay
   */
  void parseLine(const string&);

 public:
  /**
   * Constructor
   */
  AmSipDtmfEvent(const string& request_body);
};

/** the inband DTMF detector interface */
class AmInbandDtmfDetector
{
 protected:
  /** here key presses are reported to */
  AmKeyPressSink *m_keysink;

 public:
  AmInbandDtmfDetector(AmKeyPressSink *keysink);
  virtual ~AmInbandDtmfDetector() { }
  /**
   * Entry point for audio stream
   */
  virtual int streamPut(const unsigned char* samples, unsigned int size, unsigned int user_ts) = 0;
};

/**
 * \brief Inband DTMF detector
 *
 * This class implements detection of DTMF from audio stream
 */
class AmSemsInbandDtmfDetector
: public AmInbandDtmfDetector
{
 private:
  /**
   * Time when first audio packet containing current DTMF tone was detected
   */
  struct timeval m_startTime;

  static const int REL_DTMF_NPOINTS = 205;    /* Number of samples for DTMF recognition */
  static const int REL_NCOEFF = 8;            /* number of frequencies to be analyzed   */

  const int SAMPLERATE;
  /**
   * DTMF recognition successfull only if no less than DTMF_INTERVAL
   * audio packets were processed and all gave the same result
   */
  static const int DTMF_INTERVAL = 3;

  /* For DTMF recognition:
   * 2 * cos(2 * PI * k / N) precalculated for all k
   */
  int rel_cos2pik[REL_NCOEFF];

  int m_buf[REL_DTMF_NPOINTS];
  char m_last;
  int m_idx;
  int m_result[16];
  int m_lastCode;
  int m_last_ts;	// timestamp representative for the currently analysed filter block

  int m_count;

  void isdn_audio_goertzel_relative();
  void isdn_audio_eval_dtmf_relative();
  void isdn_audio_calc_dtmf(const signed short* buf, int len, unsigned int ts);

 public:
  AmSemsInbandDtmfDetector(AmKeyPressSink *keysink);
  ~AmSemsInbandDtmfDetector();
  /**
   * Entry point for audio stream
   */
  int streamPut(const unsigned char* samples, unsigned int size, unsigned int user_ts);
};


#ifdef USE_SPANDSP

class AmSpanDSPInbandDtmfDetector 
: public AmInbandDtmfDetector  {

  struct timeval key_start;
  int m_lastCode;
  dtmf_rx_state_t rx_state;

  static void tone_report_func(void *user_data, int code, int level, int delay);
  void tone_report_f(int code, int level, int delay);
  int char2int(char code);  
/*   static void dtmf_rx_callback(void* user_data, const char* digits, int len);  */
/*   void dtmf_rx_f(const char* digits, int len); */

 public: 
  AmSpanDSPInbandDtmfDetector(AmKeyPressSink *keysink);
  ~AmSpanDSPInbandDtmfDetector();

  /**
   * Entry point for audio stream
   */
  int streamPut(const unsigned char* samples, unsigned int size, unsigned int user_ts);
};
#endif // USE_SPANDSP


/**
 * \brief SIP INFO DTMF detector
 *
 * This class implements detection of DTMF from audio stream
 */
class AmSipDtmfDetector
{
 private:
  AmKeyPressSink *m_keysink;

 public:
  AmSipDtmfDetector(AmKeyPressSink *keysink);
  void process(AmSipDtmfEvent *event);
};

/**
 * \brief RTP DTMF detector
 *
 * This class implements detection of DTMF sent via RTP
 */
class AmRtpDtmfDetector
{
 private:
  /**
   *  sink for detected keys 
   */
  AmKeyPressSink *m_keysink;
  /**
   * Is there event pending?
   */
  bool m_eventPending;
  int m_currentEvent;
  int m_packetCount;

  static const int MAX_PACKET_WAIT = 3;
  /**
   * Time when first packet for current event was received
   */
  struct timeval m_startTime;

  /**
   * Send out pending event
   */
  void sendPending();

 public:
  /**
   * Constructor
   * @param keysink is the sink for detected keys 
   */
  AmRtpDtmfDetector(AmKeyPressSink *keysink);
  /**
   * Process RTP DTMF event
   */
  void process(AmRtpDtmfEvent *event);
  void checkTimeout();
};

/**
 * \brief DTMF detector class
 *
 * This class collects DTMF info from three sources: RTP (RFC 2833), 
 * SIP INFO method (RFC 2976) and DTMF tones from audio stream.
 * Received DTMF events are further reported to SEMS application via 
 * DialogState::onDtmf() call.
 */
class AmDtmfDetector 
: public AmEventHandler,
  public AmKeyPressSink
{
 private:
  static const int WAIT_TIMEOUT = 100; // miliseconds
  /**
   * Session this class belongs to.
   */
  AmSession *m_session;
  AmRtpDtmfDetector m_rtpDetector;
  AmSipDtmfDetector m_sipDetector;
  auto_ptr<AmInbandDtmfDetector> m_inbandDetector;
  Dtmf::InbandDetectorType m_inband_type;

  struct timeval m_startTime;
  struct timeval m_lastReportTime;
  int m_currentEvent;
  bool m_eventPending;

  bool m_sipEventReceived;
  bool m_inbandEventReceived;
  bool m_rtpEventReceived;

  AmMutex m_reportLock;

  /**
   * Implementation of AmEventHandler::process(). 
   * Processes events from AmMediaProcessor.
   * @see AmEventHandler
   */
  virtual void process(AmEvent *);

  void reportEvent();

  /**
   * Through this method the AmDtmfDetector receives events that was
   * detected by specific detectors.
   * @param event code of key pressed
   * @param source which detector posted this event
   * @param start time when key was pressed
   * @param stop time when key was released
   */
  void registerKeyReleased(int event, Dtmf::EventSource source,
			   const struct timeval& start, const struct timeval& stop);
  /**
   * Through this method the AmDtmfDetector receives events that was
   * detected by specific detectors.
   * @param event code of key released
   * @param source which detector posted this event
   */
  void registerKeyPressed(int event, Dtmf::EventSource source);

 public:
  /**
   * Constructor
   * @param session is the owner of this class instance
   */
  AmDtmfDetector(AmSession *session);
  virtual ~AmDtmfDetector() {}

  void checkTimeout();
  void putDtmfAudio(const unsigned char *, int size, int user_ts);

  void setInbandDetector(Dtmf::InbandDetectorType t);
  friend class AmSipDtmfDetector;
  friend class AmRtpDtmfDetector;
  friend class AmInbandDtmfDetector;
};
#endif // _AmDtmfDetector_h_
