#if defined(XMLRPC_THREADS)

#include "XmlRpcThreadedServer.h"
//#include "XmlRpcServerConnection.h"


using namespace XmlRpc;

// executeRequestThreaded:
//  remove the serverConnection from the dispatcher (but don't close the socket)
//  push the request onto the request queue 
//   (acquire the mutex, push_back request, release mutex, incr semaphore)
//  

// worker::run
//  while ! stopped
//    pop a request off the request queue (block on semaphore/decr, acquire mutex, get request, rel)
//    executeRequest (parse, run, generate response)
//    notify the serverConnection that the response is available
//    (the serverConnection needs to add itself back to the dispatcher safely - mutex)

// How do I interrupt the dispatcher if it is waiting in a select call? 
//  i) Replace select with WaitForMultipleObjects, using WSAEventSelect to associate
//     each socket with an event object, and adding an additional "signal" event.
//

#endif // XMLRPC_THREADS
