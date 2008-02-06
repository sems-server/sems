#ifndef _sip_ua_h_
#define _sip_ua_h_

class trans_bucket;
struct sip_msg;

class sip_ua
{
public:
    virtual ~sip_ua() {}
    virtual void handle_sip_request(const char* tid, sip_msg* msg)=0;
    virtual void handle_sip_reply(sip_msg* msg)=0;
};

#endif
