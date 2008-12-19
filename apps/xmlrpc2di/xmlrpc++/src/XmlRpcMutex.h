#ifndef _XMLRPCMUTEX_H_
#define _XMLRPCMUTEX_H_
//
// XmlRpc++ Copyright (c) 2002-2003 by Chris Morley
//
#if defined(_MSC_VER)
# pragma warning(disable:4786)    // identifier was truncated in debug info
#endif

namespace XmlRpc {

  //! A simple platform-independent mutex API implemented for posix and windows.
  class XmlRpcMutex {
  public:
    //! Construct a Mutex object.
    XmlRpcMutex() : _pMutex(0) {}

    //! Destroy a Mutex object.
    ~XmlRpcMutex();

    //! Wait for the mutex to be available and then acquire the lock.
    void acquire();

    //! Release the mutex.
    void release();

    //! Utility class to acquire a mutex at construction and release it when destroyed.
    struct AutoLock {
      //! Acquire the mutex at construction
      AutoLock(XmlRpcMutex& m) : _m(m) { _m.acquire(); }
      //! Release at destruction
      ~AutoLock() { _m.release(); }
      //! The mutex being held
      XmlRpcMutex& _m;
    };

  private:

    //! Native Mutex object
    void* _pMutex;

  };  // class XmlRpcMutex

}  // namespace XmlRpc

#endif	//  _XMLRPCMUTEX_H_
