#ifndef __CONNPOOL_H__
#define __CONNPOOL_H__

#include <pthread.h>
#include <string>
#include <map>
#include <stack>
#include <binrpc.h>

using namespace std;
using std::map;
using std::stack;

class ConnPool
{
    int cap; // ..acity: maximum size of the pool
    stack<int> fdStack; // array with socket file descriptors
    int size; // active connections count; =fdStack.size() + N x get()'s
    map<int, brpc_addr_t> locAddrMap; // used for PF_LOCAL connections
    pthread_mutex_t mutex; // protects access to stack with FDs
    pthread_cond_t cond; // wait when pool empty and capacity reached
    int onwait; // how many workers are waiting on cond.
    brpc_tv_t ct_timeout;

    int new_conn();

  public:
    ConnPool(const string &target, unsigned size, brpc_tv_t ct_timeout);
    ~ConnPool();

    brpc_addr_t txAddr; // binRPC address of SER;

    int get();
    void release(int fd);
    void destroy(int fd);
};

#endif /* __CONNPOOL_H__ */
