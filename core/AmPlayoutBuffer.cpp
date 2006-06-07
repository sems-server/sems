#include "AmPlayoutBuffer.h"
#include "AmAudio.h"


#define PACKET_SAMPLES 160
#define PACKET_SIZE    (PACKET_SAMPLES<<1)
#define BUFFER_SIZE    (5*PACKET_SIZE)

#define SEARCH_OFFSET  140

#define SEARCH_REGION  110
#define TEMPLATE_SEG   80
#define DELTA          5

#define TSM_MAX_SCALE  2.0
#define TSM_MIN_SCALE  0.5

#define PI 3.14 



AmPlayoutBuffer::AmPlayoutBuffer()
    : r_ts(0),w_ts(0)
{
}

void AmPlayoutBuffer::direct_write(unsigned int ts, ShortSample* buf, unsigned int len)
{
    buffer_put(w_ts,buf,len);
}

void AmPlayoutBuffer::write(u_int32_t ref_ts, u_int32_t ts, int16_t* buf, u_int32_t len)
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


AmAdaptivePlayout::AmAdaptivePlayout()
    : idx(0),
      loss_rate(ORDER_STAT_LOSS_RATE),
      wsola_off(WSOLA_START_OFF),
      shr_threshold(SHR_THRESHOLD),
      plc_cnt(0),
      short_scaled(WSOLA_SCALED_WIN)
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

void AmAdaptivePlayout::write(u_int32_t ref_ts, u_int32_t ts, 
			      int16_t* buf, u_int32_t len)
{
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

    n_len = time_scale(ts,f);
    wsola_off = old_off + n_len - len;
    
    //ts += n_len - len;

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
	    time_scale(w_ts-len,2.0);
 	}
	else {
	    do_plc = true;
	}
	plc_cnt++;
    }

    if(do_plc){

	short plc_buf[FRAMESZ];

	for(unsigned int i=0; i<2; i++){
	    
	    fec.dofe(plc_buf);
	    buffer_put(w_ts,plc_buf,FRAMESZ);
	}

	buffer_get(ts,buf,len);
    }
    else {

	buffer_get(ts,buf,len);

	for(unsigned int i=0; i<2; i++)
	    fec.addtohistory(buf + i*FRAMESZ);
    }

    return len;
}

void AmAdaptivePlayout::direct_write(unsigned int ts, ShortSample* buf, unsigned int len)
{
    buffer_put(ts+wsola_off,buf,len);
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


short* find_best_corr(short *ts, short *sr_beg,short* sr_end)
{
    // find best correlation
    float corr=0.f,best_corr=0.f;
    short *best_sr=0;
    short *sr;

    for(sr = sr_beg; sr != sr_end; sr++){
	
	corr=0.f;
	for(int i=0; i<TEMPLATE_SEG; i++)
	    corr += float(sr[i]) * float(ts[i]);

	if((best_sr == 0) || (corr > best_corr)){
	    best_corr = corr;
	    best_sr = sr;
	}
    }

    return best_sr;
}

unsigned int AmAdaptivePlayout::time_scale(unsigned int ts, float factor)
{
    short p_buf[PACKET_SAMPLES*4];
    short merge_buf[TEMPLATE_SEG];

    short *tmpl      = p_buf + PACKET_SAMPLES;
    short *p_buf_beg = p_buf;
    short *p_buf_end;

    unsigned int s     = PACKET_SAMPLES;
    unsigned int s_all = s + PACKET_SAMPLES;

    unsigned int cur_ts   = ts;
    unsigned int begin_ts = cur_ts-PACKET_SAMPLES;

    if(factor == 1.0)
	return s;

    if(factor > TSM_MAX_SCALE)
	factor = TSM_MAX_SCALE;
    else if(factor < TSM_MIN_SCALE)
	factor = TSM_MIN_SCALE;

    short *srch_beg, *srch_end, *srch;

    while(true){

	buffer_get(begin_ts,p_buf_beg,s_all);
	p_buf_end = p_buf_beg + s_all;

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

	if (srch_beg >= srch_end)
	    break;

	srch = find_best_corr(tmpl,srch_beg,srch_end);
	memcpy(merge_buf,tmpl,TEMPLATE_SEG<<1);

	float f,v;
	for(int k=0; k<TEMPLATE_SEG; k++){

	    f = 0.5 - 0.5 * cos( PI*float(k) / float(TEMPLATE_SEG) );
	    v = (float)srch[k] * f + (float)merge_buf[k] * (1.0 - f);

	    if(v > 32767.)
		v = 32767.;
	    else if(v < -32768.)
		v = -32768.;

	    merge_buf[k] = (short)v;
	}

	buffer_put( cur_ts, merge_buf, TEMPLATE_SEG);
	buffer_put( cur_ts + TEMPLATE_SEG, srch + TEMPLATE_SEG, 
		    p_buf_end - srch - TEMPLATE_SEG );

	s      += tmpl - srch;
	s_all  += tmpl - srch;

	cur_ts += TEMPLATE_SEG/2;
	tmpl   += TEMPLATE_SEG/2;

	if(p_buf_end - tmpl < TEMPLATE_SEG + DELTA)
	    break;

	float act_fact = s / (float)PACKET_SAMPLES;

	//DBG("new size = %u, ratio = %f, factor = %f\n",s, act_fact, factor);
	if((factor > 1.0) && (act_fact >= factor))
	    break;

	else if((factor < 1.0) && (act_fact <= factor))
	    break;

	else if(act_fact >= TSM_MAX_SCALE || f <= TSM_MIN_SCALE)
	    break;

    }

    return s;
}
