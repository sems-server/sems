#ifndef _SipCtrlInterface_h_
#define _SipCtrlInterface_h_

#include "sip_ua.h"

#include <string>
#include <list>
using std::string;
using std::list;

class AmSipRequest;
class AmSipReply;

//class udp_trsp;
class trans_layer;
class trans_bucket;
struct sip_msg;
struct sip_header;

#ifndef _STANDALONE

#include "AmApi.h"

class SipCtrlInterfaceFactory: public AmCtrlInterfaceFactory
{
    string         bind_addr;
    unsigned short bind_port;

public:
    SipCtrlInterfaceFactory(const string& name): AmCtrlInterfaceFactory(name) {}
    ~SipCtrlInterfaceFactory() {}

    int onLoad();

    AmCtrlInterface* instance();
};

#else

#include "AmThread.h"

#endif


class SipCtrlInterface: 

#ifndef _STANDALONE
    public AmCtrlInterface,
#else
    public AmThread,
#endif

    public sip_ua
{
    string         bind_addr;
    unsigned short bind_port;

    //udp_trsp*      udp_server;

    trans_layer*   tl;


    void prepare_routes(const list<sip_header*>& routes, string& route_field);

protected:
    void run();
    void on_stop() {}

public:
    SipCtrlInterface(const string& bind_addr, unsigned short bind_port);
    ~SipCtrlInterface(){}

    /**
     * From AmCtrlInterface
     */

    int send(const AmSipRequest &req, string &serKey);
    int send(const AmSipReply &rep);
    
#ifndef _STANDALONE
    string getContact(const string &displayName, 
		    const string &userName, const string &hostName, 
		    const string &uriParams, const string &hdrParams);
#endif
    
    void handleSipMsg(AmSipRequest &req);
    void handleSipMsg(AmSipReply &rep);
    

    /**
     * From sip_ua
     */
    void handle_sip_request(const char* tid, sip_msg* msg);
    void handle_sip_reply(sip_msg* msg);
    
};


#endif
