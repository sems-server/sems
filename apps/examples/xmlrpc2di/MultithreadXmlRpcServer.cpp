/* 
 * Written by Jae An (jae_young_an@yahoo.com) 
 *
 * adapted for SEMS by Stefan Sayer stefan.sayer at iptego.com
 */ 
#include "XmlRpcServer.h" 
#include "XmlRpcServerConnection.h" 
#include "XmlRpcServerMethod.h" 
#include "XmlRpcSocket.h" 
#include "XmlRpcUtil.h" 
#include "XmlRpcException.h" 
#include "MultithreadXmlRpcServer.h" 
 
#include "log.h"
 
using namespace XmlRpc; 
WorkerThread::WorkerThread(MultithreadXmlRpcServer* chief) 
  : runcond(false), chief(chief) {
} 
 
// call this method before calling run 
void WorkerThread::addXmlRpcSource(XmlRpcSource* source,unsigned eventMask) 
{ 
  dispatcher.addSource(source,eventMask); 
  runcond.set(true);
}  
 
void WorkerThread::run() 
{ 
  chief->reportBack(this);

  while (!is_stopped()) {
    runcond.wait_for();
    dispatcher.work(-1.0); 
  
    dispatcher.clear(); // close socket and others ...  
    runcond.set(false);
    /* tell chief we can work again */
    chief->reportBack(this);
  }
  DBG("WorkerThread stopped.\n");
} 

void WorkerThread::on_stop() {
}

MultithreadXmlRpcServer::MultithreadXmlRpcServer()
  : XmlRpcServer(), have_waiting(false)   {
}

// Wait until all the worker threads are done. 
MultithreadXmlRpcServer::~MultithreadXmlRpcServer() 
{ 
  for (std::vector<WorkerThread*>::iterator it=
	 workers.begin(); it != workers.end();it++) {
    (*it)->stop();
    (*it)->join();
    delete *it;
  }
} 

// Accept a client connection request and create a connection to 
// handle method calls from the client. 
void MultithreadXmlRpcServer::acceptConnection() 
{ 
  int s = XmlRpcSocket::accept(this->getfd()); 
  if (s < 0) 
    { 
      ERROR("MultithreadXmlRpcServer::acceptConnection: Could not accept connection (%s).", 
			XmlRpcSocket::getErrorMsg().c_str()); 
      
    }
  else if ( ! XmlRpcSocket::setNonBlocking(s))
    {
      XmlRpcSocket::close(s);
      ERROR("XmlRpcServer::acceptConnection: Could not set socket "
	    "to non-blocking input mode (%s).\n", 
	    XmlRpcSocket::getErrorMsg().c_str());
    }
  else // Notify the dispatcher to listen for input on this source when we are in work() 
    { 
      WorkerThread* thr = NULL;
      while (!thr) {
	if (!have_waiting.get()) 
	    have_waiting.wait_for();
	thr = getIdleThread();
      }
      thr->addXmlRpcSource(this->createConnection(s), XmlRpcDispatch::ReadableEvent); 
 
      //       // Uh oh.. all threads are busy...  
      //       // Just close the connection so that the rejected client would try again. 
      //       XmlRpcSocket::close(s); 
      //       XmlRpcUtil::error("MultithreadXmlRpcServer::acceptConnection: All threads are busy. Rejected a client"); 
    } 
}

void MultithreadXmlRpcServer::reportBack(WorkerThread* thr) {
  waiting_mut.lock();
  waiting.push(thr);
  have_waiting.set(true);
  waiting_mut.unlock();
}

WorkerThread* MultithreadXmlRpcServer::getIdleThread() {
  WorkerThread* res = NULL;
  waiting_mut.lock();
  if (!waiting.empty()) {
    res = waiting.front();
    waiting.pop();
  }
  have_waiting.set(!waiting.empty());
  waiting_mut.unlock();
  return res;
}

void MultithreadXmlRpcServer::createThreads(unsigned int n) {
  for (unsigned int i=0;i<n;i++) {
    WorkerThread* thr = new WorkerThread(this);
    workers.push_back(thr);
    thr->start();
  }
}
