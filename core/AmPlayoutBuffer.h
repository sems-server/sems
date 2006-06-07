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

    u_int32_t time_scale(u_int32_t ts, float factor);
    u_int32_t next_delay(u_int32_t ref_ts, u_int32_t ts);

public:

    AmAdaptivePlayout();

    void direct_write(unsigned int ts, ShortSample* buf, unsigned int len);

    void write(u_int32_t ref_ts, u_int32_t ts, int16_t* buf, u_int32_t len);

    u_int32_t read(u_int32_t ts, int16_t* buf, u_int32_t len);
};


#endif
