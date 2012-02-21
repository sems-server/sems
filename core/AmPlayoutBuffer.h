/*
 * Copyright (C) 2005-2006 iptelorg GmbH
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
/** @file AmPlayoutBuffer.h */
#ifndef _AmPlayoutBuffer_h_
#define _AmPlayoutBuffer_h_

#include "SampleArray.h"
#include "AmStats.h"
#include "LowcFE.h"
#include "AmJitterBuffer.h"
#include <set>
using std::multiset;


#define ORDER_STAT_WIN_SIZE  35
#define ORDER_STAT_LOSS_RATE 0.1

#define EXP_THRESHOLD 20
#define SHR_THRESHOLD 180

#define WSOLA_START_OFF  10 * sample_rate / 1000
#define WSOLA_SCALED_WIN 50

// the maximum packet size that will be processed (80ms)
#define MAX_PACKET_SAMPLES 80 * SYSTEM_SAMPLECLOCK_RATE / 1000
// search segments of size TEMPLATE_SEG samples (10 ms)
#define TEMPLATE_SEG   10 * sample_rate / 1000
#define STATIC_TEMPLATE_SEG   10 * SYSTEM_SAMPLECLOCK_RATE / 1000

// Maximum value: AUDIO_BUFFER_SIZE / 2
// Note: plc result get stored in our back buffer
// maximum of 80ms PLC
#define PLC_MAX_SAMPLES (4*20*sample_rate / 1000)

class AmPLCBuffer;

/** \brief base class for Playout buffer */
class AmPlayoutBuffer
{
  // Playout buffer
  SampleArrayShort buffer;

 protected:
  u_int32_t r_ts,w_ts;
  AmPLCBuffer *m_plcbuffer;

  unsigned int last_ts;
  bool         last_ts_i;

  unsigned int sample_rate;

  /** the offset RTP receive TS <-> audio_buffer TS */ 
  unsigned int   recv_offset;
  /** the recv_offset initialized ?  */ 
  bool           recv_offset_i;

  void buffer_put(unsigned int ts, ShortSample* buf, unsigned int len);
  void buffer_get(unsigned int ts, ShortSample* buf, unsigned int len);

  virtual void write_buffer(u_int32_t ref_ts, u_int32_t ts, int16_t* buf, u_int32_t len);
  virtual void direct_write_buffer(unsigned int ts, ShortSample* buf, unsigned int len);
 public:
  AmPlayoutBuffer(AmPLCBuffer *plcbuffer, unsigned int sample_rate);
  virtual ~AmPlayoutBuffer() {}

  virtual void write(u_int32_t ref_ts, u_int32_t ts, int16_t* buf, u_int32_t len, bool begin_talk);
  virtual u_int32_t read(u_int32_t ts, int16_t* buf, u_int32_t len);

  void clearLastTs() { last_ts_i = false; }
};

/** \brief adaptive playout buffer */
class AmAdaptivePlayout: public AmPlayoutBuffer
{
  // Order statistics delay estimation
  multiset<int32_t> o_stat;
  int32_t n_stat[ORDER_STAT_WIN_SIZE];
  int     idx;
  double  loss_rate;

  // adaptive WSOLA
  u_int32_t wsola_off;
  int       shr_threshold;
  MeanArray short_scaled;

  // second stage PLC
  int       plc_cnt;
  LowcFE    fec;

  // buffers
  // strech buffer
  short p_buf[MAX_PACKET_SAMPLES*4];
  // merging buffer (merge segment from strech + original seg)
  short merge_buf[STATIC_TEMPLATE_SEG];

  u_int32_t time_scale(u_int32_t ts, float factor, u_int32_t packet_len);
  u_int32_t next_delay(u_int32_t ref_ts, u_int32_t ts);

 public:

  AmAdaptivePlayout(AmPLCBuffer *, unsigned int sample_rate);

  /** write len samples beginning from timestamp ts from buf */
  void direct_write_buffer(unsigned int ts, ShortSample* buf, unsigned int len);

  /** write len samples which beginn from timestamp ts from buf
      reference ts of buffer (monotonic increasing buffer ts) is ref_ts */
  void write_buffer(u_int32_t ref_ts, u_int32_t ts, int16_t* buf, u_int32_t len);

  /** read len samples beginn from timestamp ts into buf */
  u_int32_t read(u_int32_t ts, int16_t* buf, u_int32_t len);

};

/** \brief adaptive jitter buffer */
class AmJbPlayout : public AmPlayoutBuffer
{
 private:
  AmJitterBuffer m_jb;
  unsigned int m_last_rtp_endts;

 protected:
  void direct_write_buffer(unsigned int ts, ShortSample* buf, unsigned int len);
  void prepare_buffer(unsigned int ts, unsigned int ms);

 public:
  AmJbPlayout(AmPLCBuffer *plcbuffer, unsigned int sample_rate);

  u_int32_t read(u_int32_t ts, int16_t* buf, u_int32_t len);
  void write(u_int32_t ref_ts, u_int32_t rtp_ts, int16_t* buf, u_int32_t len, bool begin_talk);
};


#endif
