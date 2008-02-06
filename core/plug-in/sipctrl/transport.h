#ifndef _transport_h_
#define _transport_h_

#include "AmThread.h"
#include <string>

using std::string;

class trans_layer;
struct sockaddr_storage;

#define SAv4(addr) \
            ((struct sockaddr_in*)addr)


class transport: public AmThread
{
 protected:
    /**
     * Transaction layer pointer.
     * This is used for received messages.
     */
    trans_layer* tl;

 public:
    transport(trans_layer* tl);

    virtual ~transport();
    
    /**
     * Binds the transport server to an address
     * @return -1 if error(s) occured.
     */
    virtual int bind(const string& address, unsigned short port)=0;

    /**
     * Sends a message.
     * @return -1 if error(s) occured.
     */
    virtual int send(const sockaddr_storage* sa, const char* msg, const int msg_len)=0;

    virtual const char* get_local_ip()=0;
    virtual unsigned short get_local_port()=0;
    virtual void copy_local_addr(sockaddr_storage* sa)=0;
};

#endif
