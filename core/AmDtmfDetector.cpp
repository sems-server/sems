/*
 * Copyright (C) 2002-2003 Fhg Fokus (inband detector code)
 * Copyright (C) 2005 Andriy I Pylypenko
 * Copyright (C) 2007 iptego GmbH
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
#include "AmDtmfDetector.h"
#include "AmSession.h"
#include "log.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <math.h>
#include <sys/time.h>

// per RFC this is 5000ms, but in reality then 
// one needs to wait 5 sec on the first keypress
// (e.g. due to a bug on recent snoms)
#define MAX_INFO_DTMF_LENGTH 1000 

//
// AmDtmfEventQueue methods
//
AmDtmfEventQueue::AmDtmfEventQueue(AmDtmfDetector *handler)
  : AmEventQueue(handler), m_detector(handler)
{
}

void AmDtmfEventQueue::processEvents()
{
  AmDtmfDetector *local_handler = reinterpret_cast<AmDtmfDetector *>(handler);
  local_handler->checkTimeout();
  AmEventQueue::processEvents();
}

void AmDtmfEventQueue::putDtmfAudio(const unsigned char *buf, int size, unsigned long long system_ts)
{
  m_detector->putDtmfAudio(buf, size, system_ts);
}

//
// AmSipDtmfEvent methods
//
AmSipDtmfEvent::AmSipDtmfEvent(const string& request_body)
  : AmDtmfEvent(Dtmf::SOURCE_SIP)
{
  parseRequestBody(request_body);
}

void AmSipDtmfEvent::parseRequestBody(const string& request_body)
{
  string::size_type start = 0;
  string::size_type stop = 0;
  while ((stop = request_body.find('\n', start)) != string::npos)
    {
      parseLine(request_body.substr(start, stop - start));
      start = stop + 1;
    }
  if (start < request_body.length())
    {
      // last chunk was not ended with '\n'
      parseLine(request_body.substr(start, string::npos));
    }
}

void AmSipDtmfEvent::parseLine(const string& line)
{
  static const string KeySignal("Signal=");
  static const string KeyDuration("Duration=");

  if (line.length() > KeySignal.length() &&
      line.substr(0, KeySignal.length()) == KeySignal)
    {
      string event(line.substr(KeySignal.length(), string::npos));
      switch (event.c_str()[0])
        {
        case '*':
	  m_event = 10;
	  break;
        case '#':
	  m_event = 11;
	  break;
        case 'A':
        case 'a':
	  m_event = 12;
	  break;
        case 'B':
        case 'b':
	  m_event = 13;
	  break;
        case 'C':
        case 'c':
	  m_event = 14;
	  break;
        case 'D':
        case 'd':
	  m_event = 15;
	  break;
        default:
	  m_event = atol(event.c_str());
        }
    }
  else if (line.length() > KeyDuration.length() &&
	   line.substr(0, KeyDuration.length()) == KeyDuration)
    {
      m_duration_msec = atol(line.substr(KeyDuration.length(), string::npos).c_str());
      if (m_duration_msec > MAX_INFO_DTMF_LENGTH)
	m_duration_msec  = MAX_INFO_DTMF_LENGTH;

    }
}

//
// AmRtpDtmfEvent methods
//
AmRtpDtmfEvent::AmRtpDtmfEvent(const dtmf_payload_t *payload, int sample_rate, unsigned int ts)
  : AmDtmfEvent(Dtmf::SOURCE_RTP)
{
  m_duration_msec = ntohs(payload->duration) * 1000 / sample_rate;
  m_e = payload->e;
  m_volume = payload->volume;
  m_event = payload->event;
  m_ts = ts;
  // RFC 2833:
  // R: This field is reserved for future use. The sender MUST set it
  // to zero, the receiver MUST ignore it.
  // m_r = payload->r;
}

//
// AmSipDtmfDetector methods
//
AmSipDtmfDetector::AmSipDtmfDetector(AmKeyPressSink *keysink)
  : m_keysink(keysink)
{
}

void AmSipDtmfDetector::process(AmSipDtmfEvent *evt)
{
  struct timeval start;
  struct timeval stop;
  gettimeofday(&start, NULL);
  // stop = start + duration
  memcpy(&stop, &start, sizeof(struct timeval));
  stop.tv_usec += evt->duration() * 1000;
  if (stop.tv_usec > 1000000)
    {
      ++stop.tv_sec;
      stop.tv_usec -= 1000000;
    }
  m_keysink->registerKeyReleased(evt->event(), Dtmf::SOURCE_SIP, start, stop);
}

//
// AmDtmfDetector methods
//
AmDtmfDetector::AmDtmfDetector(AmDtmfSink *dtmf_sink)
  : m_dtmfSink(dtmf_sink), m_rtpDetector(this),
    m_sipDetector(this),
    m_inband_type(Dtmf::SEMSInternal), m_currentEvent(-1),
    m_eventPending(false), m_current_eventid_i(false),
    m_sipEventReceived(false),
    m_inbandEventReceived(false),
    m_rtpEventReceived(false)
{
  //#ifndef USE_SPANDSP
  //  setInbandDetector(Dtmf::SEMSInternal, m_session->RTPStream()->getSampleRate());
  //#else
  //  setInbandDetector(AmConfig::DefaultDTMFDetector, m_session->RTPStream()->getSampleRate());
  //#endif
}

void AmDtmfDetector::setInbandDetector(Dtmf::InbandDetectorType t, int sample_rate) {
#ifndef USE_SPANDSP
  if (t == Dtmf::SpanDSP) {
    ERROR("trying to use spandsp DTMF detector without support for it"
	  "recompile with -D USE_SPANDSP\n");
  }
  if (!m_inbandDetector.get())
    m_inbandDetector.reset(new AmSemsInbandDtmfDetector(this, sample_rate));

  return;
#else

  if ((t != m_inband_type) || (!m_inbandDetector.get())) {
    if (t == Dtmf::SEMSInternal) {
      DBG("Setting internal DTMF detector\n");
      m_inbandDetector.reset(new AmSemsInbandDtmfDetector(this, sample_rate));
    } else { // if t == SpanDSP
      DBG("Setting spandsp DTMF detector\n");
      m_inbandDetector.reset(new AmSpanDSPInbandDtmfDetector(this, sample_rate));
    }
    m_inband_type = t;
  }
#endif
}


void AmDtmfDetector::process(AmEvent *evt)
{
  AmDtmfEvent *event = dynamic_cast<AmDtmfEvent *>(evt);
  if (NULL == event)
    return;
  switch (event->event_id)
    {
    case Dtmf::SOURCE_RTP:
      m_rtpDetector.process(dynamic_cast<AmRtpDtmfEvent *>(event));
      break;
      //        case AmDtmfEvent::INBAND:
      //            m_audioDetector.process(dynamic_cast<AmAudioDtmfEvent *>(event));
      //            break;
    case Dtmf::SOURCE_SIP:
      m_sipDetector.process(dynamic_cast<AmSipDtmfEvent *>(event));
      break;
    }
  evt->processed = true;
}

void AmDtmfDetector::flushKey(unsigned int event_id) {
  // flush the current key if it corresponds to the one with event_id
#ifdef EXCESSIVE_DTMF_DEBUGINFO
  DBG("flushKey\n");
#endif
  if (m_eventPending && m_current_eventid_i && event_id == m_current_eventid) {
#ifdef EXCESSIVE_DTMF_DEBUGINFO
    DBG("flushKey - reportEvent()\n");
#endif
    reportEvent();
  }
}

void AmDtmfDetector::registerKeyReleased(int event, Dtmf::EventSource source,
                                         const struct timeval& start,
                                         const struct timeval& stop,
					 bool has_eventid, unsigned int event_id)
{
  // Old event has not been sent yet
  // push out it now
  if ((m_eventPending && m_currentEvent != event) ||
      (m_eventPending && has_eventid && m_current_eventid_i && (event_id != m_current_eventid))) {
#ifdef EXCESSIVE_DTMF_DEBUGINFO
    DBG("event differs - reportEvent()\n");
#endif
    reportEvent();
  }

  m_eventPending = true;
  m_currentEvent = event;
  if (has_eventid) {
    m_current_eventid_i = true;
    m_current_eventid = event_id;
  }

  if(timercmp(&start,&stop,<)){
    memcpy(&m_startTime, &start, sizeof(struct timeval));
    memcpy(&m_lastReportTime, &stop, sizeof(struct timeval));
  }
  else {
    memcpy(&m_startTime, &stop, sizeof(struct timeval));
    memcpy(&m_lastReportTime, &start, sizeof(struct timeval));
  }
  switch (source)
    {
    case Dtmf::SOURCE_SIP:
      m_sipEventReceived = true;
      break;
    case Dtmf::SOURCE_RTP:
      m_rtpEventReceived = true;
      break;
    case Dtmf::SOURCE_INBAND:
      m_inbandEventReceived = true;
      break;
    default:
      break;
    }
}

void AmDtmfDetector::registerKeyPressed(int event, Dtmf::EventSource type, bool has_eventid, unsigned int event_id)
{
#ifdef EXCESSIVE_DTMF_DEBUGINFO
  DBG("registerKeyPressed(%d, .., %s, %u); m_eventPending=%s, m_currentEvent=%d, "
      "m_current_eventid=%u,m_current_eventid_i=%s\n",
      event, has_eventid?"true":"false", event_id, m_eventPending?"true":"false", 
      m_currentEvent, m_current_eventid, m_current_eventid_i?"true":"false");
#endif
  struct timeval tm;
  gettimeofday(&tm, NULL);

  if (!m_eventPending)
    {
      m_eventPending = true;
      m_currentEvent = event;
      memcpy(&m_startTime, &tm, sizeof(struct timeval));
      memcpy(&m_lastReportTime, &tm, sizeof(struct timeval));
    }
  else
    {
      // Old event has not been sent yet
      // push out it now
      if ((m_currentEvent != event) ||
	  (has_eventid && m_current_eventid_i && (event_id != m_current_eventid))) {
#ifdef EXCESSIVE_DTMF_DEBUGINFO
	DBG("event differs - reportEvent() from key pressed\n");
#endif
	reportEvent();
      }

      long delta_msec = (tm.tv_sec - m_lastReportTime.tv_sec) * 1000 +
	(tm.tv_usec - m_lastReportTime.tv_usec) / 1000;
      // SIP INFO can report stop time is in future so avoid changing 
      // m_lastReportTime during that period
      if (delta_msec > 0)
	memcpy(&m_lastReportTime, &tm, sizeof(struct timeval));


    }

  if (has_eventid) {
    m_current_eventid_i = true;
    m_current_eventid = event_id;
  }
}

void AmDtmfDetector::checkTimeout()
{
  m_rtpDetector.checkTimeout();
  if (m_eventPending)
    {
      if (m_sipEventReceived && m_rtpEventReceived && m_inbandEventReceived)
        {
	  // all three methods triggered - do not wait until timeout
	  reportEvent();
        }
      else
        {
	  // ... else wait until timeout
	  struct timeval tm;
	  gettimeofday(&tm, NULL);
	  long delta_msec = (tm.tv_sec - m_lastReportTime.tv_sec) * 1000 +
	    (tm.tv_usec - m_lastReportTime.tv_usec) / 1000;
	  if (delta_msec > WAIT_TIMEOUT)
	    reportEvent();
        }
    }
}

void AmDtmfDetector::reportEvent()
{
  m_reportLock.lock();

  if (m_eventPending) {
    long duration = (m_lastReportTime.tv_sec - m_startTime.tv_sec) * 1000 +
      (m_lastReportTime.tv_usec - m_startTime.tv_usec) / 1000;
    m_dtmfSink->postDtmfEvent(new AmDtmfEvent(m_currentEvent, duration));
    m_eventPending = false;
    m_sipEventReceived = false;
    m_rtpEventReceived = false;
    m_inbandEventReceived = false;
    m_current_eventid_i = false;
  }

  m_reportLock.unlock();
}

void AmDtmfDetector::putDtmfAudio(const unsigned char *buf, int size, unsigned long long system_ts)
{
  if (m_inbandDetector.get()) {
    m_inbandDetector->streamPut(buf, size, system_ts);
  } else {
    DBG("warning: trying to put DTMF into non-initialized DTMF detector\n");
  }
}

// AmRtpDtmfDetector methods
AmRtpDtmfDetector::AmRtpDtmfDetector(AmKeyPressSink *keysink)
  : m_keysink(keysink), m_eventPending(false), m_currentTS(0),
  m_currentTS_i(false), m_packetCount(0), m_lastTS_i(false)
{
}

void AmRtpDtmfDetector::process(AmRtpDtmfEvent *evt)
{
  if (evt && evt->volume() < 55) // From RFC 2833:
    // The range of valid DTMF is from 0 to -36 dBm0 (must
    // accept); lower than -55 dBm0 must be rejected (TR-TSY-000181,
    // ITU-T Q.24A)
    {
      m_packetCount = 0; // reset idle packet counter

      if (m_lastTS_i && m_lastTS == evt->ts()) {
	// ignore events from past key press which was already reported
#ifdef EXCESSIVE_DTMF_DEBUGINFO
	DBG("ignore RTP event ts ==%u\n", evt->ts());
#endif
	return;
      }

      if (!m_eventPending)
        {
#ifdef EXCESSIVE_DTMF_DEBUGINFO
	  DBG("new m_eventPending, event()==%d, ts=%u\n", evt->event(), evt->ts());
#endif
	  gettimeofday(&m_startTime, NULL);
	  m_eventPending = true;
	  m_currentEvent = evt->event();
	  m_currentTS = evt->ts();
	  m_currentTS_i = true;
        }
      else
        {
#ifdef EXCESSIVE_DTMF_DEBUGINFO
	  DBG("RTP event, event()==%d, m_currentEvent == %d, m_currentTS_i=%s, "
	      "evt->ts=%u, m_currentTS=%u\n",
	      evt->event(), m_currentEvent, m_currentTS_i?"true":"false", 
	      evt->ts(), m_currentTS);
#endif

	  if ((evt->event() != m_currentEvent) || 
	      (m_currentTS_i && (evt->ts() != m_currentTS)))
            {
	      // Previous event does not end correctly so send out it now...
#ifdef EXCESSIVE_DTMF_DEBUGINFO
	      DBG("flushKey %u\n", m_currentTS);
#endif
	      m_keysink->flushKey(m_currentTS);
	      // ... and reinitialize to process current event
	      gettimeofday(&m_startTime, NULL);
	      m_eventPending = true;
	      m_currentEvent = evt->event();
	      m_currentTS = evt->ts();
	      m_currentTS_i = true;
            }
        }
#ifdef EXCESSIVE_DTMF_DEBUGINFO
      DBG("registerKeyPressed %d, %u\n", m_currentEvent, m_currentTS);
#endif
      m_keysink->registerKeyPressed(m_currentEvent, Dtmf::SOURCE_RTP, true, m_currentTS);
    }
}

void AmRtpDtmfDetector::sendPending()
{
  if (m_eventPending)
    {
      struct timeval end_time;
      gettimeofday(&end_time, NULL);
#ifdef EXCESSIVE_DTMF_DEBUGINFO
      DBG("registerKeyReleased(%d, ... %u)\n", m_currentEvent, m_currentTS);
#endif
      m_keysink->registerKeyReleased(m_currentEvent, Dtmf::SOURCE_RTP, m_startTime, end_time, true, m_currentTS);
      m_eventPending = false;
      m_currentTS_i = false;
      m_lastTS = m_currentTS;
      m_lastTS_i = true;     
    }
}

void AmRtpDtmfDetector::checkTimeout()
{
  if (m_eventPending && m_packetCount++ > MAX_PACKET_WAIT)
    {
#ifdef EXCESSIVE_DTMF_DEBUGINFO
      DBG("idle timeout ... sendPending()\n");
#endif
      sendPending();
    }
}

//
// AmInbandDtmfDetector methods

AmInbandDtmfDetector::AmInbandDtmfDetector(AmKeyPressSink *keysink) 
 : m_keysink(keysink)
{
}

//
// -------------------------------------------------------------------------------------------
#define IVR_DTMF_ASTERISK 10
#define IVR_DTMF_HASH     11
#define IVR_DTMF_A        12
#define IVR_DTMF_B        13 
#define IVR_DTMF_C        14 
#define IVR_DTMF_D        15

/* the detector returns these values */

static int IVR_dtmf_matrix[4][4] =
  {
    {                1, 2, 3,             IVR_DTMF_A},
    {                4, 5, 6,             IVR_DTMF_B},
    {                7, 8, 9,             IVR_DTMF_C},
    {IVR_DTMF_ASTERISK, 0, IVR_DTMF_HASH, IVR_DTMF_D}
  };


#define LOGRP             0
#define HIGRP             1

#define REL_DTMF_TRESH     4000     /* above this is dtmf                         */
#define REL_SILENCE_TRESH   200     /* below this is silence                      */
#define REL_AMP_BITS          9     /* bits per sample, reduced to avoid overflow */
#define PI              3.1415926
#define NELEMSOF(x) (sizeof(x)/sizeof(*x))

/** \brief DTMF tone filter type */
typedef struct {
  int freq;			/* frequency */
  int grp;			/* low/high group */
} dtmf_t;

static dtmf_t dtmf_tones[8] = 
  {
    { 697, LOGRP},
    { 770, LOGRP},
    { 852, LOGRP},
    { 941, LOGRP},
    {1209, HIGRP},
    {1336, HIGRP},
    {1477, HIGRP},
    {1633, HIGRP}
  };

static char dtmf_matrix[4][4] =
  {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
  };

AmSemsInbandDtmfDetector::AmSemsInbandDtmfDetector(AmKeyPressSink *keysink, int sample_rate)
  : AmInbandDtmfDetector(keysink),
    SAMPLERATE(sample_rate),
    m_last(' '),
    m_idx(0),
    m_count(0)
{
  /* precalculate 2 * cos (2 PI k / N) */
  for(unsigned i = 0; i < NELEMSOF(rel_cos2pik); i++) {
    // FIXME: fixed samplerate. won't work for wideband
    int k = (int)((double)dtmf_tones[i].freq * REL_DTMF_NPOINTS / SAMPLERATE + 0.5);
    rel_cos2pik[i] = (int)(2 * 32768 * cos(2 * PI * k / REL_DTMF_NPOINTS));
  }
}
AmSemsInbandDtmfDetector::~AmSemsInbandDtmfDetector() {
}

/*
 * Goertzel algorithm.
 * See http://ptolemy.eecs.berkeley.edu/~pino/Ptolemy/papers/96/dtmf_ict/
 * for more info.
 */

void AmSemsInbandDtmfDetector::isdn_audio_goertzel_relative()
{
  int sk, sk1, sk2;

  for (int k = 0; k < REL_NCOEFF; k++) {
    // like m_buf, sk..sk2 are in (32-REL_AMP_BITS).REL_AMP_BITS fixed-point format
    sk = sk1 = sk2 = 0;
    for (int n = 0; n < REL_DTMF_NPOINTS; n++) {
      sk = m_buf[n] + ((rel_cos2pik[k] * sk1) >> 15) - sk2;
      sk2 = sk1;
      sk1 = sk;
    }
    /* Avoid overflows */
    sk >>= 1;
    sk2 >>= 1;
    /* compute |X(k)|**2 */
    /* report overflows. This should not happen. */
    /* Comment this out if desired */
    /*if (sk < -32768 || sk > 32767)
      DBG("isdn_audio: dtmf goertzel overflow, sk=%d\n", sk);
      if (sk2 < -32768 || sk2 > 32767)
      DBG("isdn_audio: dtmf goertzel overflow, sk2=%d\n", sk2);
    */

    // note that the result still is in (32-REL_AMP_BITS).REL_AMP_BITS format
    m_result[k] =
      ((sk * sk) >> REL_AMP_BITS) -
      ((((rel_cos2pik[k] * sk) >> 15) * sk2) >> REL_AMP_BITS) +
      ((sk2 * sk2) >> REL_AMP_BITS);
  }
}


void AmSemsInbandDtmfDetector::isdn_audio_eval_dtmf_relative()
{
  int silence;
  int grp[2];
  char what;
  int thresh;

  grp[LOGRP] = grp[HIGRP] = -1;
  silence = 0;
  thresh = 0;

  for (int i = 0; i < REL_NCOEFF; i++) 
    {
      if (m_result[i] > REL_DTMF_TRESH) {
	if (m_result[i] > thresh)
	  thresh = m_result[i];
      }
      else if (m_result[i] < REL_SILENCE_TRESH)
	silence++;
    }
  if (silence == REL_NCOEFF)
    what = ' ';
  else {
    if (thresh > 0)	{
      thresh = thresh >> 4;  /* touchtones must match within 12 dB */

      for (int i = 0; i < REL_NCOEFF; i++) {
	if (m_result[i] < thresh)
	  continue;  /* ignore */

	/* good level found. This is allowed only one time per group */
	if (i < REL_NCOEFF / 2) {
	  /* lowgroup*/
	  if (grp[LOGRP] >= 0) {
	    // Bad. Another tone found. */
	    grp[LOGRP] = -1;
	    break;
	  }
	  else
	    grp[LOGRP] = i;
	}
	else { /* higroup */
	  if (grp[HIGRP] >= 0) { // Bad. Another tone found. */
	    grp[HIGRP] = -1;
	    break;
	  }
	  else
	    grp[HIGRP] = i - REL_NCOEFF/2;
	}
      }

      if ((grp[LOGRP] >= 0) && (grp[HIGRP] >= 0)) {
	what = dtmf_matrix[grp[LOGRP]][grp[HIGRP]];
	m_lastCode = IVR_dtmf_matrix[grp[LOGRP]][grp[HIGRP]];
		
	if (what != m_last)
	  {
	    m_startTime.tv_sec = m_last_ts / SAMPLERATE;
	    m_startTime.tv_usec = ((m_last_ts * 10000) / (SAMPLERATE/100)) 
	      % 1000000;
	  }
      } else
	what = '.';
    }
    else
      what = '.';
  }
  if (what != ' ' && what != '.')
    {
      if (++m_count >= DTMF_INTERVAL)
        {
	  m_keysink->registerKeyPressed(m_lastCode, Dtmf::SOURCE_INBAND);
        }
    }
  else
    {
      if (m_last != ' ' && m_last != '.' && m_count >= DTMF_INTERVAL)
        {
	  struct timeval stop;
	  stop.tv_sec = m_last_ts / SAMPLERATE;
	  stop.tv_usec = ((m_last_ts * 10000) / (SAMPLERATE/100)) % 1000000;
	  m_keysink->registerKeyReleased(m_lastCode, Dtmf::SOURCE_INBAND, m_startTime, stop);
        }
      m_count = 0;
    }
  m_last = what;
}

void AmSemsInbandDtmfDetector::isdn_audio_calc_dtmf(const signed short* buf, int len, unsigned int ts)
{
  int c;

  if(m_idx == 0) m_last_ts = ts;

  while (len) {
    c = (len < ((int)NELEMSOF(m_buf) - m_idx)) ? len : (NELEMSOF(m_buf) - m_idx);

    if (c <= 0) break;

    for (int i = 0; i < c; i++) {
      // m_buf is in (32-REL_AMP_BITS).REL_AMP_BITS fixed-point format, the samples
      // itself are in the last REL_AMP_BITS bits, i.e. they go from -1.0 to +1.0
      // (or more exactly from -1.0 to ~+0.996)
      m_buf[m_idx++] = (*buf++) >> (15 - REL_AMP_BITS);
    }
    if (m_idx == NELEMSOF(m_buf)) {
      isdn_audio_goertzel_relative();
      isdn_audio_eval_dtmf_relative();
      m_idx = 0;
      m_last_ts = ts + c;
    }
    len -= c;
    ts += c;
  }
}

int AmSemsInbandDtmfDetector::streamPut(const unsigned char* samples, unsigned int size, unsigned long long system_ts)
{
  unsigned long long user_ts =
    system_ts * ((unsigned long long)SAMPLERATE / 100)
    / (WALLCLOCK_RATE / 100);

  isdn_audio_calc_dtmf((const signed short *)samples, size / 2, (unsigned int)user_ts);
  return size;
}

#ifdef USE_SPANDSP

AmSpanDSPInbandDtmfDetector::AmSpanDSPInbandDtmfDetector(AmKeyPressSink *keysink, int sample_rate)
  : AmInbandDtmfDetector(keysink) 
{
#ifdef HAVE_OLD_SPANDSP_CALLBACK
  rx_state = (dtmf_rx_state_t*)malloc(sizeof(dtmf_rx_state_t));
  if (NULL == rx_state) {
    throw string("error allocating memory for DTMF detector\n");
  }
#else
  rx_state = NULL;
#endif
  rx_state = dtmf_rx_init(rx_state, NULL /* dtmf_rx_callback */, (void*)this); 

  if (rx_state==NULL) {
    throw string("error allocating memory for DTMF detector\n");
  }
  dtmf_rx_set_realtime_callback(rx_state, tone_report_func, (void*)this); 
}

AmSpanDSPInbandDtmfDetector::~AmSpanDSPInbandDtmfDetector() {
  // not in 0.0.4:
  //  dtmf_rx_release(rx_state);
#ifdef HAVE_OLD_SPANDSP_CALLBACK
  free(rx_state);
#else
  dtmf_rx_free(rx_state);
#endif
}

int AmSpanDSPInbandDtmfDetector::streamPut(const unsigned char* samples, unsigned int size, unsigned long long system_ts) {
  dtmf_rx(rx_state, (const int16_t*) samples, size/2);
  return size;
}

#ifndef HAVE_OLD_SPANDSP_CALLBACK
void AmSpanDSPInbandDtmfDetector::tone_report_func(void *user_data, int code, int level, int delay) {
  AmSpanDSPInbandDtmfDetector* o = (AmSpanDSPInbandDtmfDetector*)user_data;
  o->tone_report_f(code, level, delay);
}
#else
void AmSpanDSPInbandDtmfDetector::tone_report_func(void *user_data, int code) {
  AmSpanDSPInbandDtmfDetector* o = (AmSpanDSPInbandDtmfDetector*)user_data;
  o->tone_report_f(code, 0, 0);
}
#endif

void AmSpanDSPInbandDtmfDetector::tone_report_f(int code, int level, int delay) {
  //  DBG("spandsp reports tone %c, %d, %d\n", code, level, delay);
  if (code) { // key pressed
    gettimeofday(&key_start, NULL);
    m_lastCode = code;
    // don't report key press - otherwise reported twice
    //    m_keysink->registerKeyPressed(char2int(code), Dtmf::SOURCE_INBAND);
  } else { // released
    struct timeval now;
    gettimeofday(&now, NULL);
    m_keysink->registerKeyReleased(char2int(m_lastCode), Dtmf::SOURCE_INBAND, key_start, now);    
  }
}

// uh, ugly
int AmSpanDSPInbandDtmfDetector::char2int(char code) {
  if (code>='0' && code<='9') 
    return code-'0';
  if (code == '#') 
    return IVR_DTMF_HASH;
  if (code == '*') 
    return IVR_DTMF_ASTERISK;

  if (code >= 'A' && code <= 'D')
    return code-'A';
  return code;
}

// unused - we use realtime reporting functions instead
// void AmSpanDSPInbandDtmfDetector::dtmf_rx_callback(void* user_data, const char* digits, int len) {
//   AmSpanDSPInbandDtmfDetector* o = (AmSpanDSPInbandDtmfDetector*)user_data;
//   o->dtmf_rx_f(digits, len);
// }

// void AmSpanDSPInbandDtmfDetector::dtmf_rx_f(const char* digits, int len) {
//   DBG("dtmf_rx_callback len=%d\n", len);

//   for (int i=0;i<len;i++)
//     DBG("char %c\n", digits[i]);
// }
#endif // USE_SPANDSP
