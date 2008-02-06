#ifndef _udp_trsp_h_
#define _udp_trsp_h_

#include "transport.h"

/**
 * Maximum message length for UDP
 * not including terminating '\0'
 */
#define MAX_UDP_MSGLEN 65535

#include <sys/socket.h>

#include <string>
using std::string;

class udp_trsp: public transport
{
    // socket descriptor
    int sd;

    // bound port number
    unsigned short local_port;

    // bound IP
    string local_ip;

    // bound address
    sockaddr_storage local_addr;

 protected:
    /** @see AmThread */
    void run();
    /** @see AmThread */
    void on_stop();

 public:
    /** @see transport */
    udp_trsp(trans_layer* tl);
    ~udp_trsp();

    /** @see transport */
    int bind(const string& address, unsigned short port);

    /** @see transport */
    int send(const sockaddr_storage* sa, const char* msg, const int msg_len);

    const char* get_local_ip();
    unsigned short get_local_port();
    void copy_local_addr(sockaddr_storage* sa);
};

#endif
