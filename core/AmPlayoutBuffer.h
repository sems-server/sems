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

#define WSOLA_START_OFF  80
#define WSOLA_SCALED_WIN 50

// the maximum packet size that will be processed
//   640 is 80ms @ 8khz
#define MAX_PACKET_SAMPLES 640
// search segments of size TEMPLATE_SEG samples 
#define TEMPLATE_SEG   80

// Maximum value: AUDIO_BUFFER_SIZE / 2
// Note: plc result get stored in our back buffer
#define PLC_MAX_SAMPLES (160*4) 

class AmRtpAudio;

/** \brief base class for Playout buffer */
class AmPlayoutBuffer
{
    // Playout buffer
    SampleArrayShort buffer;

protected:
    u_int32_t r_ts,w_ts;
    AmRtpAudio *m_owner;

    unsigned int last_ts;
    bool         last_ts_i;

    /** the offset RTP receive TS <-> audio_buffer TS */ 
    unsigned int   recv_offset;
    /** the recv_offset initialized ?  */ 
    bool           recv_offset_i;

    void buffer_put(unsigned int ts, ShortSample* buf, unsigned int len);
    void buffer_get(unsigned int ts, ShortSample* buf, unsigned int len);

    virtual void write_buffer(u_int32_t ref_ts, u_int32_t ts, int16_t* buf, u_int32_t len);
    virtual void direct_write_buffer(unsigned int ts, ShortSample* buf, unsigned int len);
public:
    AmPlayoutBuffer(AmRtpAudio *owner);
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
    short merge_buf[TEMPLATE_SEG];

    u_int32_t time_scale(u_int32_t ts, float factor, u_int32_t packet_len);
    u_int32_t next_delay(u_int32_t ref_ts, u_int32_t ts);

public:

    AmAdaptivePlayout(AmRtpAudio *);

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
    AmJbPlayout(AmRtpAudio *owner);

    u_int32_t read(u_int32_t ts, int16_t* buf, u_int32_t len);
    void write(u_int32_t ref_ts, u_int32_t rtp_ts, int16_t* buf, u_int32_t len, bool begin_talk);
};


#endif
