
#ifndef __BRPCCTRLINTERFACE_H__
#define __BRPCCTRLINTERFACE_H__

#include <time.h>
#include <binrpc.h>

#include "AmApi.h"
#include "AmSipDispatcher.h"

class BrpcCtrlInterface : public AmCtrlInterface
{
  private:
    static BrpcCtrlInterface *_instance;
    //addresses for:
    //- SEMS listening for SER requests
    //- SER listening for SEMS requests
    //- SEMS SeNDing requests to SER, when using PF_LOCAL sockets (might not
    //be used) 
    brpc_addr_t semsAddr, serAddr, sndAddr;
    int semsFd, serFd;
    // handler of requests (SIP request | reply) received from SER
    AmInterfaceHandler *sipDispatcher;
    //what load returned; if strictly positive, load() hadn't been called, yet
    int loaded;

    inline void handleSipMsg(AmSipRequest &req)
    {
      sipDispatcher->handleSipMsg(req);
    }
    inline void handleSipMsg(AmSipReply &rpl)
    {
      sipDispatcher->handleSipMsg(rpl);
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
    int load();

  public:
    static time_t serial;
    static brpc_int_t as_id;

    BrpcCtrlInterface(const string &name);
    ~BrpcCtrlInterface();

    AmCtrlInterface *instance() { return _instance; }

    // AmThread
    void run();
    void on_stop() {}
    // AmPluginFactory
    int onLoad();
    // AmCtrlInterface
    int send(const AmSipRequest &, string &);
    int send(const AmSipReply &);

    string localURI(const string &displayName, 
        const string &userName, const string &hostName, 
        const string &uriParams, const string &hdrParams);

    void registerInterfaceHandler(AmInterfaceHandler *handler)
    {
      sipDispatcher = handler;
    }
};

#endif /* __BRPCCTRLINTERFACE_H__ */
