/*
 * $Id: AmDtmfDetector.cpp,v 1.1.2.1 2005/06/01 12:00:24 rco Exp $
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
#include "AmDtmfDetector.h"
#include "AmSession.h"
#include "log.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <math.h>
#include <sys/time.h>

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

void AmDtmfEventQueue::putDtmfAudio(const unsigned char *buf, int size, int user_ts)
{
    m_detector->putDtmfAudio(buf, size, user_ts);
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
    }
}

//
// AmRtpDtmfEvent methods
//
AmRtpDtmfEvent::AmRtpDtmfEvent(const dtmf_payload_t *payload, int sample_rate)
    : AmDtmfEvent(Dtmf::SOURCE_RTP)
{
    m_duration_msec = ntohs(payload->duration) * 1000 / sample_rate;
    m_e = payload->e;
    m_volume = payload->volume;
    m_event = payload->event;
    // RFC 2833:
    // R: This field is reserved for future use. The sender MUST set it
    // to zero, the receiver MUST ignore it.
    // m_r = payload->r;
}

//
// AmSipDtmfDetector methods
//
AmSipDtmfDetector::AmSipDtmfDetector(AmDtmfDetector *owner)
    : m_owner(owner)
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
    m_owner->registerKeyReleased(evt->event(), Dtmf::SOURCE_SIP, start, stop);
}

//
// AmDtmfDetector methods
//
AmDtmfDetector::AmDtmfDetector(AmSession *session)
    : m_session(session), m_rtpDetector(this),
      m_inbandDetector(this), m_sipDetector(this),
      m_eventPending(false), m_sipEventReceived(false),
      m_inbandEventReceived(false), m_rtpEventReceived(false)
{
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

void AmDtmfDetector::registerKeyReleased(int event, Dtmf::EventSource source,
                                         const struct timeval& start,
                                         const struct timeval& stop)
{
    // Old event has not been sent yet
    // push out it now
    if (m_eventPending && m_currentEvent != event)
    {
        reportEvent();
    }
    m_eventPending = true;
    m_currentEvent = event;
    memcpy(&m_startTime, &start, sizeof(struct timeval));
    memcpy(&m_lastReportTime, &stop, sizeof(struct timeval));
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

void AmDtmfDetector::registerKeyPressed(int event, Dtmf::EventSource type)
{
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
        long delta_msec = (tm.tv_sec - m_lastReportTime.tv_sec) * 1000 +
                            (tm.tv_usec - m_lastReportTime.tv_usec) / 1000;
        // SIP INFO can report stop time is in future so avoid changing 
        // m_lastReportTime during that period
        if (delta_msec > 0)
            memcpy(&m_lastReportTime, &tm, sizeof(struct timeval));
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

    long duration = (m_lastReportTime.tv_sec - m_startTime.tv_sec) * 1000 +
            (m_lastReportTime.tv_usec - m_startTime.tv_usec) / 1000;
    m_session->postDtmfEvent(new AmDtmfEvent(m_currentEvent, duration));
    m_eventPending = false;
    m_sipEventReceived = false;
    m_rtpEventReceived = false;
    m_inbandEventReceived = false;

    m_reportLock.unlock();
}

void AmDtmfDetector::putDtmfAudio(const unsigned char *buf, int size, int user_ts)
{
    m_inbandDetector.streamPut(buf, size, user_ts);
}

// AmRtpDtmfDetector methods
AmRtpDtmfDetector::AmRtpDtmfDetector(AmDtmfDetector *owner)
    : m_owner(owner), m_eventPending(false), m_packetCount(0)
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
        if (!m_eventPending)
        {
            gettimeofday(&m_startTime, NULL);
            m_eventPending = true;
            m_currentEvent = evt->event();
        }

        if (m_eventPending)
        {
            if (evt->event() != m_currentEvent)
            {
                // Previous event does not end correctly so send out it now...
                sendPending();
                // ... and reinitialize to process current event
                gettimeofday(&m_startTime, NULL);
                m_eventPending = true;
                m_currentEvent = evt->event();
            }
            if (evt->e())
            {
                sendPending();
            }
        }
        if (m_eventPending)
        {
            m_owner->registerKeyPressed(m_currentEvent, Dtmf::SOURCE_RTP);
        }
    }
}

void AmRtpDtmfDetector::sendPending()
{
    if (m_eventPending)
    {
        struct timeval end_time;
        gettimeofday(&end_time, NULL);
        m_owner->registerKeyReleased(m_currentEvent, Dtmf::SOURCE_RTP, m_startTime, end_time);
        m_eventPending = false;
    }
}

void AmRtpDtmfDetector::checkTimeout()
{
    if (m_eventPending && m_packetCount++ > MAX_PACKET_WAIT)
    {
        sendPending();
    }
}

//
// AmInbandDtmfDetector methods
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


#define DTMF_TRESH   100000     /* above this is dtmf                         */
#define SILENCE_TRESH   100     /* below this is silence                      */
#define H2_TRESH      35000	/* 2nd harmonic                               */
#define DTMF_DUR         25	/* nr of samples to detect a DTMF tone        */
#define AMP_BITS          9     /* bits per sample, reduced to avoid overflow */
#define NCOEFF           16     /* number of frequencies to be analyzed       */
#define LOGRP             0
#define HIGRP             1

#define REL_NCOEFF            8     /* number of frequencies to be analyzed       */
#define REL_DTMF_TRESH     4000     /* above this is dtmf                         */
#define REL_SILENCE_TRESH   200     /* below this is silence                      */
#define REL_AMP_BITS          9     /* bits per sample, reduced to avoid overflow */
#define PI              3.1415926


typedef struct {
    int freq;			/* frequency */
    int grp;			/* low/high group */
    int k;			/* k */
    int k2;			/* k for 1st harmonic */
} dtmf_t;


static int cos2pik[NCOEFF] = {		/* 2 * cos(2 * PI * k / N), precalculated */
    55812,      29528,      53603,      24032,
    51193,      14443,      48590,       6517
}; // high group missing...

/* For DTMF recognition:
 * 2 * cos(2 * PI * k / N) precalculated for all k
 */
static int rel_cos2pik[REL_NCOEFF] =
{
    55813, 53604, 51193, 48591,      // low group
    38114, 33057, 25889, 18332       // high group
};

static dtmf_t dtmf_tones[8] = 
{
    { 697, LOGRP,  0,  1 },
    { 770, LOGRP,  2,  3 },
    { 852, LOGRP,  4,  5 },
    { 941, LOGRP,  6,  7 },
    {1209, HIGRP,  8,  9 },
    {1336, HIGRP, 10, 11 },
    {1477, HIGRP, 12, 13 },
    {1633, HIGRP, 14, 15 }
};

static char dtmf_matrix[4][4] =
{
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

AmInbandDtmfDetector::AmInbandDtmfDetector(AmDtmfDetector *owner)
    : m_owner(owner),
      m_last(' '),
      m_idx(0),
      m_count(0)
{
    /* precalculate 2 * cos (2 PI k / N) */
    int i, kp, k;
    kp = 0;
    for(i = 0; i < 8; i++) 
    {
        k = (int)rint((double)dtmf_tones[i].freq * DTMF_NPOINTS / SAMPLERATE);
        cos2pik[kp] = (int)(2 * 32768 * cos(2 * PI * k / DTMF_NPOINTS));
        dtmf_tones[i].k = kp++;

        k = (int)rint((double)dtmf_tones[i].freq * 2 * DTMF_NPOINTS / SAMPLERATE);
        cos2pik[kp] = (int)(2 * 32768 * cos(2 * PI * k / DTMF_NPOINTS));
        dtmf_tones[i].k2 = kp++;
    }
}

/*
 * Goertzel algorithm.
 * See http://ptolemy.eecs.berkeley.edu/~pino/Ptolemy/papers/96/dtmf_ict/
 * for more info.
 */

void AmInbandDtmfDetector::isdn_audio_goertzel_relative()
{
    int sk, sk1, sk2;

    for (int k = 0; k < REL_NCOEFF; k++) 
    {
        sk = sk1 = sk2 = 0;
        for (int n = 0; n < REL_DTMF_NPOINTS; n++) 
        {
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

        m_result[k] =
                ((sk * sk) >> REL_AMP_BITS) -
                ((((rel_cos2pik[k] * sk) >> 15) * sk2) >> REL_AMP_BITS) +
                ((sk2 * sk2) >> REL_AMP_BITS);
    }
}


void AmInbandDtmfDetector::isdn_audio_eval_dtmf_relative()
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
                    gettimeofday(&m_startTime, NULL);
                }
            } else
                what = '.';
        }
        else
            what = '.';
    }
    if (what != ' ' && what != '.')
    {
        if (m_count++ >= DTMF_INTERVAL)
        {
            m_owner->registerKeyPressed(m_lastCode, Dtmf::SOURCE_INBAND);
        }
    }
    else
    {
        if (m_last != ' ' && m_last != '.' && m_count >= DTMF_INTERVAL)
        {
            struct timeval stop;
            gettimeofday(&stop, NULL);
            m_owner->registerKeyReleased(m_lastCode, Dtmf::SOURCE_INBAND, m_startTime, stop);
        }
        m_count = 0;
    }
    m_last = what;
}

void AmInbandDtmfDetector::isdn_audio_calc_dtmf(const signed short* buf, int len)
{
    int c;

    while (len)
    {
        c = (len < (DTMF_NPOINTS - m_idx)) ? len : (DTMF_NPOINTS - m_idx);

        if (c <= 0)
            break;

        for (int i = 0; i < c; i++) {
            m_buf[m_idx++] = (*buf++) >> (15 - AMP_BITS);
        }
        if (m_idx == DTMF_NPOINTS) 
        {
            isdn_audio_goertzel_relative();
            isdn_audio_eval_dtmf_relative();
	    m_idx = 0;
        }
        len -= c;
    }
}

int AmInbandDtmfDetector::streamPut(const unsigned char* samples, unsigned int size, unsigned int)
{
    isdn_audio_calc_dtmf((const signed short *)samples, size / 2);
    return size;
}
