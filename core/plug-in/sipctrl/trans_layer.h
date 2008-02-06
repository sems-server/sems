#ifndef _trans_layer_h_
#define _trans_layer_h_

//#include "AmApi.h"

#include "cstring.h"

#include <list>
using std::list;

struct sip_msg;
struct sip_uri;
struct sip_trans;
struct sip_header;
struct sockaddr_storage;

class MyCtrlInterface;
class trans_bucket;
class udp_trsp;
class sip_ua;
class timer;


class trans_layer
{
    /** 
     * Singleton pointer.
     * @see instance()
     */
    static trans_layer* _instance;

    sip_ua*   ua;
    udp_trsp* transport;
    
    
    /** Avoid external instantiation. @see instance(). */
    trans_layer();

    /** Avoid external instantiation. @see instance(). */
    ~trans_layer();

    /**
     * Implements the state changes for the UAC state machine
     * @return -1 if errors
     * @return transaction state if successfull
     */
    int update_uac_trans(trans_bucket* bucket, sip_trans* t, sip_msg* msg);

    /**
     * Implements the state changes for the UAS state machine
     */
    int update_uas_request(trans_bucket* bucket, sip_trans* t, sip_msg* msg);
    int update_uas_reply(trans_bucket* bucket, sip_trans* t, int reply_code);

    /**
     * Retransmits reply / non-200 ACK (if possible).
     */
    void retransmit(sip_trans* t);

    /**
     * Retransmits a message (mostly the first UAC request).
     */
    void retransmit(sip_msg* msg);

    /**
     * Send ACK coresponding to error replies
     */
    void send_non_200_ack(sip_trans* t, sip_msg* reply);
    
    /**
     * Fills the address structure passed and modifies 
     * R-URI and Route headers as needed.
     */
    int set_next_hop(list<sip_header*>& route_hdrs, cstring& r_uri, 
		     sockaddr_storage* remote_ip);

 public:

    /**
     * Retrieve the singleton instance.
     */
    static trans_layer* instance();

    /**
     * Register a SIP UA.
     * This method MUST be called ONCE.
     */
    void register_ua(sip_ua* ua);

    /**
     * Register a transport instance.
     * This method MUST be called ONCE.
     */
    void register_transport(udp_trsp* trsp);

    /**
     * Sends a UAS reply.
     * If a body is included, the hdrs parameter should
     * include a well-formed 'Content-Type', but no
     * 'Content-Length' header.
     */
    int send_reply(trans_bucket* bucket, sip_trans* t,
		   int reply_code, const cstring& reason,
		   const cstring& to_tag, const cstring& contact,
		   const cstring& hdrs, const cstring& body);

    /**
     * Sends a UAC request.
     * Caution: Route headers should not be added to the
     * general header list (msg->hdrs).
     * @param msg Pre-built message.
     */
    int send_request(sip_msg* msg);

    /**
     * Called by the transport layer
     * when a new message has been recived.
     */
    void received_msg(sip_msg* msg);

    /**
     * Sends an end-to-end ACK for the reply
     * passed as a parameter.
     */
    void send_200_ack(sip_msg* reply);


    /**
     * This is called by the transaction timer callback.
     * At this place, the bucket is already locked, so
     * please be quick.
     */
    void timer_expired(timer* t, trans_bucket* bucket, sip_trans* tr);
};



#endif // _trans_layer_h_
