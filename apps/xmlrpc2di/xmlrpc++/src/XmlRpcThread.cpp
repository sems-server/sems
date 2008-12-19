#if defined(XMLRPC_THREADS)

#include "XmlRpcThread.h"

#if defined(_WINDOWS)
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <process.h>
#else
# include <pthread.h>
#endif


using namespace XmlRpc;


//! Destructor. Does not perform a join() (ie, the thread may continue to run).
XmlRpcThread::~XmlRpcThread()
{
  if (_pThread)
  {
#if defined(_WINDOWS)
    ::CloseHandle((HANDLE)_pThread);
#else
    ::pthread_detach((pthread_t)_pThread);
#endif
    _pThread = 0;
  }
}

//! Execute the run method of the runnable object in a separate thread.
//! Returns immediately in the calling thread.
void
XmlRpcThread::start()
{
  if ( ! _pThread)
  {
#if defined(_WINDOWS)
    unsigned threadID;
    _pThread = (HANDLE)_beginthreadex(NULL, 0, &runInThread, this, 0, &threadID);
#else
    ::pthread_create((pthread_t*) &_pThread, NULL, &runInThread, this);
#endif
  }
}

//! Waits until the thread exits.
void
XmlRpcThread::join()
{
  if (_pThread)
  {
#if defined(_WINDOWS)
    ::WaitForSingleObject(_pThread, INFINITE);
    ::CloseHandle(_pThread);
#else
    ::pthread_join((pthread_t)_pThread, 0);
#endif
    _pThread = 0;
  }
}

//! Start the runnable going in a thread
unsigned int
XmlRpcThread::runInThread(void* pThread)
{
  XmlRpcThread* t = (XmlRpcThread*)pThread;
  t->getRunnable()->run();
  return 0;
}

#endif // XMLRPC_THREADS


