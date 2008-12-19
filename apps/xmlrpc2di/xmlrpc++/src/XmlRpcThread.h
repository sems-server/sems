#ifndef _XMLRPCTHREAD_H_
#define _XMLRPCTHREAD_H_
//
// XmlRpc++ Copyright (c) 2002-2003 by Chris Morley
//
#if defined(_MSC_VER)
# pragma warning(disable:4786)    // identifier was truncated in debug info
#endif

namespace XmlRpc {

  //! An abstract class providing an interface for objects that can run in a separate thread.
  class XmlRpcRunnable {
  public:
      //! Code to be executed.
      virtual void run() = 0;
  };  // class XmlRpcRunnable


  //! A simple platform-independent thread API implemented for posix and windows.
  class XmlRpcThread {
  public:
    //! Construct a thread object. Not usable until setRunnable() has been called.
    XmlRpcThread() : _runner(0), _pThread(0) {}

    //! Construct a thread object.
    XmlRpcThread(XmlRpcRunnable* runnable) : _runner(runnable), _pThread(0) {}

    //! Destructor. Does not perform a join() (ie, the thread may continue to run).
    ~XmlRpcThread();

    //! Execute the run method of the runnable object in a separate thread.
    //! Returns immediately in the calling thread.
    void start();

    //! Waits until the thread exits.
    void join();

    //! Access the runnable
    XmlRpcRunnable* getRunnable() const { return _runner; }

    //! Set the runnable
    void setRunnable(XmlRpcRunnable* r) { _runner = r; }

  private:

    //! Start the runnable going in a thread
    static unsigned int __stdcall runInThread(void* pThread);

    //! Code to be executed
    XmlRpcRunnable* _runner;

    //! Native thread object
    void* _pThread;

  };  // class XmlRpcThread

}  // namespace XmlRpc

#endif	//  _XMLRPCTHREAD_H_
