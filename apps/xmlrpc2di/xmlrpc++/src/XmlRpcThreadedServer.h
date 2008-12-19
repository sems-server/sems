
#ifndef _XMLRPCTHREADEDSERVER_H_
#define _XMLRPCTHREADEDSERVER_H_
//
// XmlRpc++ Copyright (c) 2002-2003 by Chris Morley
//
#if defined(_MSC_VER)
# pragma warning(disable:4786)    // identifier was truncated in debug info
#endif

#ifndef MAKEDEPEND
# include <map>
# include <vector>
#endif


#include "XmlRpcMutex.h"
#include "XmlRpcServer.h"
#include "XmlRpcThread.h"


namespace XmlRpc {

  //! A class to handle multiple simultaneous XML RPC requests
  class XmlRpcThreadedServer : public XmlRpcServer {
  public:

    //! Create a server object with a specified number of worker threads.
    XmlRpcThreadedServer(int nWorkers = 6) : _workers(nWorkers) {}


    //! Execute a request

  protected:

    //! Each client request is assigned to one worker to handle.
    //! Workers are executed on separate threads, and one worker may be
    //! responsible for dispatching events to multiple client connections.
    class Worker : XmlRpcRunnable {
    public:
      //! Constructor. Executes the run method in a separate thread.
      Worker() { _thread.setRunnable(this); _thread.start(); }

      //! Implement the Runnable interface
      void run();

    protected:

      //! The thread this worker is running in.
      XmlRpcThread _thread;

    };


    //! The worker pool
    std::vector<Worker> _workers;


    //! Serialize dispatcher access
    XmlRpcMutex _mutex;


  };  // class XmlRpcThreadedServer

}

#endif  // _XMLRPCTHREADEDSERVER_H_
