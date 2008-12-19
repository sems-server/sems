#include "XmlRpc.h"
#include "TOXmlRpcClient.h"
// Clear the referenced flag even if exceptions or errors occur.
struct ClearFlagOnExit {
  ClearFlagOnExit(bool& flag) : _flag(flag) {}
  ~ClearFlagOnExit() { _flag = false; }
  bool& _flag;
};

bool TOXmlRpcClient::execute(const char* method, XmlRpcValue const& params, XmlRpcValue& result, double timeout) {

  XmlRpcUtil::log(1, "XmlRpcClient::execute: method %s (_connectionState %d).", method, _connectionState);

  // This is not a thread-safe operation, if you want to do multithreading, use separate
  // clients for each thread. If you want to protect yourself from multiple threads
  // accessing the same client, replace this code with a real mutex.
  if (_executing)
    return false;

  _executing = true;
  ClearFlagOnExit cf(_executing);

  _sendAttempts = 0;
  _isFault = false;

  if ( ! setupConnection())
    return false;

  if ( ! generateRequest(method, params))
    return false;

  result.clear();
  //  double msTime = -1.0;   // Process until exit is called
  _disp.work(timeout);

  if (_connectionState != IDLE || ! parseResponse(result))
    return false;

  XmlRpcUtil::log(1, "XmlRpcClient::execute: method %s completed.", method);
  _response = "";
  return true;
}
