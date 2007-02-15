#ifndef _AmPlayoutBuffer_h_
#define _AmPlayoutBuffer_h_

#include "SampleArray.h"
#include "AmStats.h"
#include "LowcFE.h"
#include <set>
using std::multiset;


#define ORDER_STAT_WIN_SIZE  35
#define ORDER_STAT_LOSS_RATE 0.1

#define EXP_THRESHOLD 20
#define SHR_THRESHOLD 180

#define WSOLA_START_OFF  80
#define WSOLA_SCALED_WIN 50

// the maximum packet size that will be processed
//   640 is 80ms @ 8khz
#define MAX_PACKET_SAMPLES 640
// search segments of size TEMPLATE_SEG samples 
#define TEMPLATE_SEG   80

/** \brief base class for Playout buffer */
class AmPlayoutBuffer
{
    // Playout buffer
    SampleArrayShort buffer;

protected:
    u_int32_t r_ts,w_ts;

    void buffer_put(unsigned int ts, ShortSample* buf, unsigned int len);
    void buffer_get(unsigned int ts, ShortSample* buf, unsigned int len);
    
public:
    AmPlayoutBuffer();
    virtual ~AmPlayoutBuffer() {}

    virtual void direct_write(unsigned int ts, ShortSample* buf, unsigned int len);
    virtual void write(u_int32_t ref_ts, u_int32_t ts, int16_t* buf, u_int32_t len);
    virtual u_int32_t read(u_int32_t ts, int16_t* buf, u_int32_t len);
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
    short merge_buf[TEMPLATE_SEG];

    u_int32_t time_scale(u_int32_t ts, float factor, u_int32_t packet_len);
    u_int32_t next_delay(u_int32_t ref_ts, u_int32_t ts);

public:

    AmAdaptivePlayout();

    /** write len samples beginning from timestamp ts from buf */
    void direct_write(unsigned int ts, ShortSample* buf, unsigned int len);

    /** write len samples which beginn from timestamp ts from buf
	reference ts of buffer (monotonic increasing buffer ts) is ref_ts */
    void write(u_int32_t ref_ts, u_int32_t ts, int16_t* buf, u_int32_t len);

    /** read len samples beginn from timestamp ts into buf */
    u_int32_t read(u_int32_t ts, int16_t* buf, u_int32_t len);
};


#endif
