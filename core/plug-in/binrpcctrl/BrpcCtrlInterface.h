
#ifndef __BRPCCTRLINTERFACE_H__
#define __BRPCCTRLINTERFACE_H__

#include <time.h>
#include <binrpc.h>

#include "AmApi.h"
#include "AmSipDispatcher.h"

class BrpcCtrlInterfaceFactory : public AmCtrlInterfaceFactory
{
    string semsUri, serUri;

  public:
    BrpcCtrlInterfaceFactory(const string &name);
    ~BrpcCtrlInterfaceFactory();

    AmCtrlInterface *instance();

    // AmPluginFactory
    int onLoad();
};

class BrpcCtrlInterface: public AmCtrlInterface
{
    /* static */ time_t serial;
    /* static */ brpc_int_t as_id;

    // handler of requests (SIP request | reply) received from SER
    AmSipDispatcher *sipDispatcher;

    //addresses for:
    //- SEMS listening for SER requests
    //- SER listening for SEMS requests
    //- SEMS SeNDing requests to SER, when using PF_LOCAL sockets (might not
    //be used) 
    brpc_addr_t semsAddr, serAddr, sndAddr;
    int semsFd, serFd;

    inline void handleSipMsg(AmSipRequest &req)
    {
	AmSipDispatcher::instance()->handleSipMsg(req);
    }

    inline void handleSipMsg(AmSipReply &rpl)
    {
	AmSipDispatcher::instance()->handleSipMsg(rpl);
    }

    brpc_t *rpcExecute(brpc_t *req);
    bool rpcCheck();
    void serResync();

    static brpc_t *asiSync(brpc_t *req, void *iface);
    static brpc_t *methods(brpc_t *req, void *iface);
    static brpc_t *digests(brpc_t *req, void *iface);
    static brpc_t *req_handler(brpc_t *req, void *iface);

    int getSerFd();
    void closeSerConn();
    static void closeSock(int *sock, brpc_addr_t *addr);

    bool initCallbacks();
    void _run();

 public:
    BrpcCtrlInterface();
    ~BrpcCtrlInterface();

    int init(const string& semsUri, const string& serUri);

    // AmThread
    void run();
    void on_stop() {}

    // AmCtrlInterface
    int send(const AmSipRequest &, string &);
    int send(const AmSipReply &);

    string getContact(const string &displayName, 
        const string &userName, const string &hostName, 
        const string &uriParams, const string &hdrParams);


};

#endif /* __BRPCCTRLINTERFACE_H__ */
