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

#include "AmPlayoutBuffer.h"
#include "AmAudio.h"
#include "AmRtpAudio.h"

#define SEARCH_OFFSET  140

#define SEARCH_REGION  110
#define DELTA          5

#define TSM_MAX_SCALE  2.0
#define TSM_MIN_SCALE  0.5

// only scale if 0.9 < f < 1.1
#define SCALE_FACTOR_START 0.1

#define PI 3.14 

#define MAX_DELAY sample_rate*1 /* 1 second */

AmPlayoutBuffer::AmPlayoutBuffer(AmPLCBuffer *plcbuffer, unsigned int sample_rate)
  : r_ts(0),w_ts(0), m_plcbuffer(plcbuffer),
    last_ts_i(false), sample_rate(sample_rate),
    recv_offset_i(false)
{
  buffer.clear_all();
}

void AmPlayoutBuffer::direct_write_buffer(unsigned int ts, ShortSample* buf, unsigned int len)
{
  buffer_put(w_ts,buf,len);
}

void AmPlayoutBuffer::write(u_int32_t ref_ts, u_int32_t rtp_ts, 
			    int16_t* buf, u_int32_t len, bool begin_talk)
{  
  unsigned int mapped_ts;
  if(!recv_offset_i)
    {
      recv_offset = rtp_ts - ref_ts;
      recv_offset_i = true;
      DBG("initialized recv_offset with %u (%u - %u)\n",
	  recv_offset, ref_ts, rtp_ts);
      mapped_ts = r_ts = w_ts = ref_ts;
    }
  else {
    mapped_ts = rtp_ts - recv_offset;

    // resync
    if( ts_less()(mapped_ts, ref_ts - MAX_DELAY/2) || 
	!ts_less()(mapped_ts, ref_ts + MAX_DELAY) ){

      DBG("resync needed: reference ts = %u; write ts = %u\n",
	  ref_ts, mapped_ts);
      recv_offset = rtp_ts - ref_ts;
      mapped_ts = r_ts = w_ts = ref_ts;
    }
  }

  if(!last_ts_i)
    {
      last_ts = mapped_ts;
      last_ts_i = true;
    }

  if(ts_less()(last_ts, mapped_ts) && !begin_talk
     && (mapped_ts - last_ts <= PLC_MAX_SAMPLES))
    {
      unsigned char tmp[AUDIO_BUFFER_SIZE * 2];
      int l_size = m_plcbuffer->conceal_loss(mapped_ts - last_ts, tmp);
      if (l_size>0)
        {
	  direct_write_buffer(last_ts, (ShortSample*)tmp, PCM16_B2S(l_size));
        }
    }
  m_plcbuffer->add_to_history(buf, PCM16_S2B(len));

  write_buffer(ref_ts, mapped_ts, buf, len);

  // update last_ts to end of received packet 
  // if not out-of-sequence
  if (ts_less()(last_ts, mapped_ts) || last_ts == mapped_ts)
    last_ts = mapped_ts + len;
}


void AmPlayoutBuffer::write_buffer(u_int32_t ref_ts, u_int32_t ts, int16_t* buf, u_int32_t len)
{
  buffer_put(w_ts,buf,len);
}

u_int32_t AmPlayoutBuffer::read(u_int32_t ts, int16_t* buf, u_int32_t len)
{
  if(ts_less()(r_ts,w_ts)){

    u_int32_t rlen=0;
    if(ts_less()(r_ts+PCM16_B2S(AUDIO_BUFFER_SIZE),w_ts))
      rlen = PCM16_B2S(AUDIO_BUFFER_SIZE);
    else
      rlen = w_ts - r_ts;

    buffer_get(r_ts,buf,rlen);
    return rlen;
  }

  return 0;
}


void AmPlayoutBuffer::buffer_put(unsigned int ts, ShortSample* buf, unsigned int len)
{
  buffer.put(ts,buf,len);

  if(ts_less()(w_ts,ts+len))
    w_ts = ts + len;
}

void AmPlayoutBuffer::buffer_get(unsigned int ts, ShortSample* buf, unsigned int len)
{
  buffer.get(ts,buf,len);

  if(ts_less()(r_ts,ts+len))
    r_ts = ts + len;
}

//
// See: Y. J. Liang, N. Farber, and B. Girod. Adaptive playout scheduling 
// and loss concealment for voice communication over IP networks. Submitted 
// to IEEE Transactions on Multimedia, Feb. 2001. 
// Online at: 
//  http://www-ise.stanford.edu/yiliang/publications/ 
//  http://citeseer.ist.psu.edu/liang02adaptive.html 
// 
AmAdaptivePlayout::AmAdaptivePlayout(AmPLCBuffer *plcbuffer, unsigned int sample_rate)
  : AmPlayoutBuffer(plcbuffer, sample_rate),
    idx(0),
    loss_rate(ORDER_STAT_LOSS_RATE),
    wsola_off(WSOLA_START_OFF),
    shr_threshold(SHR_THRESHOLD),
    short_scaled(WSOLA_SCALED_WIN),
    plc_cnt(0),
    fec(sample_rate)
{
  memset(n_stat,0,sizeof(int32_t)*ORDER_STAT_WIN_SIZE);
}

u_int32_t AmAdaptivePlayout::next_delay(u_int32_t ref_ts, u_int32_t ts)
{
  int32_t n = (int32_t)(ref_ts - ts);
    
  multiset<int32_t>::iterator it = o_stat.find(n_stat[idx]);
  if(it != o_stat.end())
    o_stat.erase(it);

  n_stat[idx] = n;
  o_stat.insert(n);
    

  int32_t D_r=0,D_r1=0;
  int r = int((double(o_stat.size()) + 1.0)*(1.0 - loss_rate));
    
  if((r == 0) || (r >= (int)o_stat.size())){

    StddevValue n_std;
    for(int i=0; i<ORDER_STAT_WIN_SIZE; i++){
      n_std.push(double(n_stat[i]));
    }
	
    if(r == 0){
      D_r = (*o_stat.begin()) - (int32_t)(2.0*n_std.stddev());
      D_r1 = (*o_stat.begin());
    }
    else {
      D_r = (*o_stat.rbegin());
      D_r1 = (*o_stat.rbegin()) + (int32_t)(2.0*n_std.stddev());
    }

	
  }
  else {
    int i=0;
    for(it = o_stat.begin(); it != o_stat.end(); it++){
	    
      if(++i == r){
	D_r = (*it);
	++it;
	D_r1 = (*it);
	break;
      }
    }
  }

  int32_t D = 
    int32_t(D_r + double(D_r1 - D_r)
	    * ( (double(o_stat.size()) + 1.0)
		*(1.0-loss_rate) - double(r)));

  if(++idx >= ORDER_STAT_WIN_SIZE)
    idx = 0;

  return D;
}

void AmAdaptivePlayout::write_buffer(u_int32_t ref_ts, u_int32_t ts, 
				     int16_t* buf, u_int32_t len)
{
  // predict next delay
  u_int32_t p_delay = next_delay(ref_ts,ts);

  u_int32_t old_off = wsola_off;
  ts += old_off;

  if(short_scaled.mean() > 2.0){
    if(shr_threshold < 3000)
      shr_threshold += 10;
  }
  else if(short_scaled.mean() < 1.0){
    if(shr_threshold > 100)
      shr_threshold -= 2;
  }

  // need to scale?
  if( ts_less()(wsola_off+EXP_THRESHOLD,p_delay) ||   // expand packet
      ts_less()(p_delay+shr_threshold,wsola_off) )  { // shrink packet

    wsola_off = p_delay;
  }
  else {
    if(ts_less()(r_ts,ts+len)){
      plc_cnt = 0;
      buffer_put(ts,buf,len);
    }
    else {
      // lost
    }

    // statistics
    short_scaled.push(0.0);

    return;
  }

  int32_t n_len = len + wsola_off - old_off;
  if(n_len < 0)
    n_len = 1;

  float f = float(n_len) / float(len);
  if(f > TSM_MAX_SCALE)
    f = TSM_MAX_SCALE;

  n_len = (int32_t)(float(len) * f);
  if(ts_less()(ts+n_len,r_ts)){

    // statistics
    short_scaled.push(0.0);
    return;
  }
    
  u_int32_t old_wts = w_ts;
  buffer_put(ts,buf,len);

  n_len = time_scale(ts,f,len);
  wsola_off = old_off + n_len - len;
   
  // if we have shrinked the voice, set back w_ts 
  // in order to have correct start point for possible
  // PLC
  if (n_len < (int32_t) len)
    w_ts += n_len - len;

  if(w_ts != old_wts)
    plc_cnt = 0;

  // statistics
  short_scaled.push(100.0);
}

u_int32_t AmAdaptivePlayout::read(u_int32_t ts, int16_t* buf, u_int32_t len)
{
  bool do_plc=false;

  if(ts_less()(w_ts,ts+len) && (plc_cnt < 6)){
	
    if(!plc_cnt){
      int nlen = time_scale(w_ts-len,2.0, len);
      wsola_off += nlen-len;
    }
    else {
      do_plc = true;
    }
    plc_cnt++;
  }

  if(do_plc){

    short plc_buf[FRAMESZ];

    for(unsigned int i=0; i<len/FRAMESZ; i++){
	    
      fec.dofe(plc_buf);

      buffer_put(w_ts,plc_buf,FRAMESZ);
    }

    buffer_get(ts,buf,len);
  }
  else {

    buffer_get(ts,buf,len);

    for(unsigned int i=0; i<len/FRAMESZ; i++)
      fec.addtohistory(buf + i*FRAMESZ);
  }

  return len;
}

void AmAdaptivePlayout::direct_write_buffer(unsigned int ts, ShortSample* buf, unsigned int len)
{
  buffer_put(ts+wsola_off,buf,len);
}

/** 
 * find best cross correlation of a TEMPLATE_SEG samples
 * long frame 
 *  * starting between sr_beg ... sr_end
 *  * to TEMPLATE_SEG samples frame starting from ts
 * 
 */
short* find_best_corr(short *ts, short *sr_beg, short* sr_end, unsigned int sample_rate)
{
  // find best correlation
  float corr=0.f,best_corr=0.f;
  short *best_sr=ts;
  short *sr;

  for(sr = sr_beg; sr != sr_end; sr++){
	
    corr=0.f;
    for(unsigned int i=0; i<TEMPLATE_SEG; i++)
      corr += float(sr[i]) * float(ts[i]);

    if((best_sr == 0) || (corr > best_corr)){
      best_corr = corr;
      best_sr = sr;
    }
  }

  return best_sr;
}

u_int32_t AmAdaptivePlayout::time_scale(u_int32_t ts, float factor, 
					u_int32_t packet_len)
{
  // current position in strech buffer 
  short *tmpl      = p_buf + packet_len;
  // begin and end of strech buffer
  short *p_buf_beg = p_buf;
  short *p_buf_end;

  // initially size is packet_len
  unsigned int s     = packet_len;

  // we start from beginning of frame
  unsigned int cur_ts   = ts;

  // safety
  if (packet_len > MAX_PACKET_SAMPLES)
    return s;
  
  // not possible to stretch packets shorter than 10ms 
  if (packet_len < TEMPLATE_SEG)
      return s;

  if (fabs(factor - 1.0) <= SCALE_FACTOR_START) {
#ifdef DEBUG_PLAYOUTBUF
    DBG("not scaling - too little f difference \n");
#endif
    return s;
  }

  // boundaries of scaling 
  if(factor > TSM_MAX_SCALE)
    factor = TSM_MAX_SCALE;
  else if(factor < TSM_MIN_SCALE)
    factor = TSM_MIN_SCALE;

  short *srch_beg, *srch_end, *srch;

  while(true){
    // get previous packet_len frame + scaled frame 
    // (with size s) into p_buf
    buffer_get(ts - packet_len, p_buf_beg, s + packet_len);
    p_buf_end = p_buf_beg + s + packet_len;

    // determine search region for template seg
    // as srch_beg ... srch_end
    if (factor > 1.0){
      // expansion
      srch_beg = tmpl - (int)((float)TEMPLATE_SEG * (factor - 1.0)) - SEARCH_REGION/2; 
      srch_end = srch_beg + SEARCH_REGION;

      if(srch_beg < p_buf_beg)
	srch_beg = p_buf_beg;

      if(srch_end + DELTA >= tmpl)
	srch_end = tmpl - DELTA;
    }
    else {
      // compression
      srch_end = tmpl + (int)((float)TEMPLATE_SEG * (1.0 - factor)) + SEARCH_REGION/2;
      srch_beg = srch_end - SEARCH_REGION;
	    
      if(srch_end + TEMPLATE_SEG > p_buf_end)
	srch_end = p_buf_end - TEMPLATE_SEG;
	    
      if(srch_beg - DELTA < tmpl)
	srch_beg = tmpl + DELTA;
    }
    // stop if search region size < 0
    if (srch_beg >= srch_end)
      break;

    // find best correlation to tmpl in srch_beg..srch_end
    srch = find_best_corr(tmpl,srch_beg,srch_end,sample_rate);

    // merge original segment (starting from tmpl) and 
    // best correlation (starting from srch) into merge_buf 
    float f = 0.5,v = 0.5;
    for(unsigned int k=0; k<TEMPLATE_SEG; k++){

      f = 0.5 - 0.5 * cos( PI*float(k) / float(TEMPLATE_SEG) );
      v = (float)srch[k] * f + (float)tmpl[k] * (1.0 - f);

      if(v > 32767.)
	v = 32767.;
      else if(v < -32768.)
	v = -32768.;

      merge_buf[k] = (short)v;
    }

    // put merged segment into buffer
    buffer_put( cur_ts, merge_buf, TEMPLATE_SEG);
    if (p_buf_end - srch - TEMPLATE_SEG < 0) {
      ERROR("audio after merged segment spills over\n");
      break;
    }
    // add after merged segment audio from after srch 
    buffer_put( cur_ts + TEMPLATE_SEG, srch + TEMPLATE_SEG, 
		p_buf_end - srch - TEMPLATE_SEG );
    // size s has changed
    s      += tmpl - srch;

    // go to next segment
    cur_ts += TEMPLATE_SEG/2;
    tmpl   += TEMPLATE_SEG/2;

    // calculate current factor
    float act_fact = s / (float)packet_len;

#ifdef DEBUG_PLAYOUTBUF
    DBG("at ts %u: new size = %u, ratio = %f, requested = %f (wsola_off = %ld)\n", 
	ts, s, act_fact, factor, (long)wsola_off);
#endif
    // break condition: coming to the end of the frame (with safety margin)
    if((unsigned int)(p_buf_end - tmpl) < TEMPLATE_SEG + DELTA)
      break;

    // streched enough?
    if((factor > 1.0) && (act_fact >= factor))
      break;
    else if((factor < 1.0) && (act_fact <= factor))
      break;

    // streched over maximum already?
    else if(act_fact >= TSM_MAX_SCALE || f <= TSM_MIN_SCALE)
      break;

  }

  return s;
}

/*****************************************************************
 *
 *  AmJbPlayout class methods
 *
 *****************************************************************/

AmJbPlayout::AmJbPlayout(AmPLCBuffer *plcbuffer, unsigned int sample_rate)
  : AmPlayoutBuffer(plcbuffer, sample_rate)
{
}

u_int32_t AmJbPlayout::read(u_int32_t ts, int16_t* buf, u_int32_t len)
{
  prepare_buffer(ts, len);
  buffer_get(ts, buf, len);
  return len;
}

void AmJbPlayout::direct_write_buffer(unsigned int ts, ShortSample* buf, unsigned int len)
{
  buffer_put(ts, buf, len);
}

void AmJbPlayout::prepare_buffer(unsigned int audio_buffer_ts, unsigned int ms)
{
  ShortSample buf[AUDIO_BUFFER_SIZE * 10];
  unsigned int ts;
  unsigned int nb_samples;
  /**
   * Get all RTP packets that correspond to the required interval,
   * decode them and put into playout buffer.
   */
  while (m_jb.get(audio_buffer_ts, ms, buf, &nb_samples, &ts))
    {
      direct_write_buffer(ts, buf, nb_samples);
      m_plcbuffer->add_to_history(buf, PCM16_S2B(nb_samples));
      /* Conceal the gap between previous and current RTP packets */
      if (last_ts_i && ts_less()(m_last_rtp_endts, ts))
       	{
	  int concealed_size = m_plcbuffer->conceal_loss(ts - m_last_rtp_endts, (unsigned char *)buf);
	  if (concealed_size > 0)
	    direct_write_buffer(m_last_rtp_endts, buf, PCM16_B2S(concealed_size));
	}
      m_last_rtp_endts = ts + nb_samples;
      last_ts_i = true;
    }
  if (!last_ts_i) {
    return;
  }
  if (ts_less()(m_last_rtp_endts, audio_buffer_ts + ms))
    {
      /* Last packets have been lost. Conceal them */
      int concealed_size = m_plcbuffer->conceal_loss(audio_buffer_ts + ms - m_last_rtp_endts, (unsigned char *)buf);
      if (concealed_size > 0)
	direct_write_buffer(m_last_rtp_endts, buf, PCM16_B2S(concealed_size));
      m_last_rtp_endts = audio_buffer_ts + ms;
    }
}

void AmJbPlayout::write(u_int32_t ref_ts, u_int32_t rtp_ts, int16_t* buf, u_int32_t len, bool begin_talk)
{
  m_jb.put(buf, len, rtp_ts, begin_talk);
}
