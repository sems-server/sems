#ifndef __CTRLSERVER_H__
#define __CTRLSERVER_H__

#include <string>
#include <binrpc.h>
#include "AmThread.h"

using namespace std;

class CtrlWorker : public AmThread
{
    volatile int running; //don't like the shared var of AmThread (locking)
    brpc_tv_t tx_timeout, rx_timeout;
    int rxFd;
    brpc_addr_t rxAddr;

    void run();
    void on_stop();

  public:
    CtrlWorker();

    void init(int rxFd, brpc_addr_t rxAddr, 
        brpc_tv_t rx_timeout, brpc_tv_t tx_timeout);
};

/**
 * This is just a proxy class, handling multiple receiver threads
 */
class CtrlServer
{
    int rxFd;
    CtrlWorker *workers;
    unsigned wcnt;

  public:
    CtrlServer(const string &listen, unsigned listeners, 
        brpc_tv_t rx_timeout, brpc_tv_t tx_timeout);
    ~CtrlServer();

    brpc_addr_t rxAddr;

    void start();
    void stop();
    void join();
};

#endif /* __CTRLSERVER_H__ */
