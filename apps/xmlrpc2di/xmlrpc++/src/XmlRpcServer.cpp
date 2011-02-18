

#include "XmlRpcServer.h"
#include "XmlRpcServerConnection.h"
#include "XmlRpcServerMethod.h"
#include "XmlRpcSocket.h"
#include "XmlRpcUtil.h"
#include "XmlRpcException.h"
#include "XmlRpc.h"


using namespace XmlRpc;


// Static data
const char XmlRpcServer::METHODNAME_TAG[] = "<methodName>";
const char XmlRpcServer::PARAMS_TAG[] = "<params>";
const char XmlRpcServer::PARAMS_ETAG[] = "</params>";
const char XmlRpcServer::PARAM_TAG[] = "<param>";
const char XmlRpcServer::PARAM_ETAG[] = "</param>";

const std::string XmlRpcServer::METHODNAME = "methodName";
const std::string XmlRpcServer::PARAMS = "params";

const std::string XmlRpcServer::FAULTCODE = "faultCode";
const std::string XmlRpcServer::FAULTSTRING = "faultString";



XmlRpcServer::XmlRpcServer()
{
  _introspectionEnabled = false;
  _listMethods = 0;
  _methodHelp = 0;
}


XmlRpcServer::~XmlRpcServer()
{
  this->shutdown();
  _methods.clear();
  delete _listMethods;
  delete _methodHelp;
}


// Add a command to the RPC server
void 
XmlRpcServer::addMethod(XmlRpcServerMethod* method)
{
  _methods[method->name()] = method;
}

// Remove a command from the RPC server
void 
XmlRpcServer::removeMethod(XmlRpcServerMethod* method)
{
  MethodMap::iterator i = _methods.find(method->name());
  if (i != _methods.end())
    _methods.erase(i);
}

// Remove a command from the RPC server by name
void 
XmlRpcServer::removeMethod(const std::string& methodName)
{
  MethodMap::iterator i = _methods.find(methodName);
  if (i != _methods.end())
    _methods.erase(i);
}


// Look up a method by name
XmlRpcServerMethod* 
XmlRpcServer::findMethod(const std::string& name) const
{
  MethodMap::const_iterator i = _methods.find(name);
  if (i == _methods.end())
    return 0;
  return i->second;
}


// Create a socket, bind to the specified port, and
// set it in listen mode to make it available for clients.
bool 
XmlRpcServer::bindAndListen(int port, const std::string& bind_ip, int backlog /*= 5*/)
{
  int fd = XmlRpcSocket::socket();
  if (fd < 0)
  {
    XmlRpcUtil::error("XmlRpcServer::bindAndListen: Could not create socket (%s).", XmlRpcSocket::getErrorMsg().c_str());
    return false;
  }

  this->setfd(fd);

  // Don't block on reads/writes
  if ( ! XmlRpcSocket::setNonBlocking(fd))
  {
    this->close();
    XmlRpcUtil::error("XmlRpcServer::bindAndListen: Could not set socket to non-blocking input mode (%s).", XmlRpcSocket::getErrorMsg().c_str());
    return false;
  }

  // Allow this port to be re-bound immediately so server re-starts are not delayed
  if ( ! XmlRpcSocket::setReuseAddr(fd))
  {
    this->close();
    XmlRpcUtil::error("XmlRpcServer::bindAndListen: Could not set SO_REUSEADDR socket option (%s).", XmlRpcSocket::getErrorMsg().c_str());
    return false;
  }

  // Bind to the specified port on the default interface
  if ( ! XmlRpcSocket::bind(fd, port, bind_ip))
  {
    this->close();
    XmlRpcUtil::error("XmlRpcServer::bindAndListen: Could not bind to specified port (%s).", XmlRpcSocket::getErrorMsg().c_str());
    return false;
  }

  // Set in listening mode
  if ( ! XmlRpcSocket::listen(fd, backlog))
  {
    this->close();
    XmlRpcUtil::error("XmlRpcServer::bindAndListen: Could not set socket in listening mode (%s).", XmlRpcSocket::getErrorMsg().c_str());
    return false;
  }

  XmlRpcUtil::log(2, "XmlRpcServer::bindAndListen: server listening on port %d fd %d", port, fd);

  // Notify the dispatcher to listen on this source when we are in work()
  _disp.addSource(this, XmlRpcDispatch::ReadableEvent);

  return true;
}


// Get port number that this server is listening on
int
XmlRpcServer::getPort(void) const
{
  return XmlRpcSocket::getPort(getfd());
}



// Process client requests for the specified time
void 
XmlRpcServer::work(double msTime)
{
  XmlRpcUtil::log(2, "XmlRpcServer::work: waiting for a connection");
  _disp.work(msTime);
}



// Handle input on the server socket by accepting the connection
// and reading the rpc request.
unsigned
XmlRpcServer::handleEvent(unsigned mask)
{
  acceptConnection();
  return XmlRpcDispatch::ReadableEvent;		// Continue to monitor this fd
}


// Accept a client connection request and create a connection to
// handle method calls from the client.
void
XmlRpcServer::acceptConnection()
{
  int s = XmlRpcSocket::accept(this->getfd());
  XmlRpcUtil::log(2, "XmlRpcServer::acceptConnection: socket %d", s);
  if (s < 0)
  {
    //this->close();
    XmlRpcUtil::error("XmlRpcServer::acceptConnection: Could not accept connection (%s).", XmlRpcSocket::getErrorMsg().c_str());
  }
  else if ( ! XmlRpcSocket::setNonBlocking(s))
  {
    XmlRpcSocket::close(s);
    XmlRpcUtil::error("XmlRpcServer::acceptConnection: Could not set socket to non-blocking input mode (%s).", XmlRpcSocket::getErrorMsg().c_str());
  }
  else  // Notify the dispatcher to listen for input on this source when we are in work()
  {
    XmlRpcUtil::log(2, "XmlRpcServer::acceptConnection: creating a connection");
    XmlRpcServerConnection* c = this->createConnection(s);
    if (c) this->dispatchConnection(c);
  }
}


// Create a new connection object for processing requests from a specific client.
XmlRpcServerConnection*
XmlRpcServer::createConnection(int s)
{
  // Specify that the connection object be deleted when it is closed
  return new XmlRpcServerConnection(s, this, true);
}


// Hand off a new connection to a dispatcher
void
XmlRpcServer::dispatchConnection(XmlRpcServerConnection* sc)
{
  _disp.addSource(sc, XmlRpcDispatch::ReadableEvent);
}


// Remove a connection. Called by the connection when it closes down.
void 
XmlRpcServer::removeConnection(XmlRpcServerConnection* sc)
{
  _disp.removeSource(sc);
}


// Stop processing client requests
void 
XmlRpcServer::exit()
{
  _disp.exit();
}


// Close the server socket file descriptor and stop monitoring connections
void 
XmlRpcServer::shutdown()
{
  // This closes and destroys all connections as well as closing this socket
  _disp.clear();
}


// Introspection support
static const std::string LIST_METHODS("system.listMethods");
static const std::string METHOD_HELP("system.methodHelp");
static const std::string MULTICALL("system.multicall");


// List all methods available on a server
class ListMethods : public XmlRpcServerMethod
{
public:
  ListMethods(XmlRpcServer* s) : XmlRpcServerMethod(LIST_METHODS, s) {}

  void execute(XmlRpcValue& params, XmlRpcValue& result)
  {
    _server->listMethods(result);
  }

  std::string help() { return std::string("List all methods available on a server as an array of strings"); }
};


// Retrieve the help string for a named method
class MethodHelp : public XmlRpcServerMethod
{
public:
  MethodHelp(XmlRpcServer* s) : XmlRpcServerMethod(METHOD_HELP, s) {}

  void execute(XmlRpcValue& params, XmlRpcValue& result)
  {
    if (params[0].getType() != XmlRpcValue::TypeString)
      throw XmlRpcException(METHOD_HELP + ": Invalid argument type");

    XmlRpcServerMethod* m = _server->findMethod(params[0]);
    if ( ! m)
      throw XmlRpcException(METHOD_HELP + ": Unknown method name");

    result = m->help();
  }

  std::string help() { return std::string("Retrieve the help string for a named method"); }
};

    
// Specify whether introspection is enabled or not. Default is enabled.
void 
XmlRpcServer::enableIntrospection(bool enabled)
{
  if (_introspectionEnabled == enabled)
    return;

  _introspectionEnabled = enabled;

  if (enabled)
  {
    if ( ! _listMethods)
    {
      _listMethods = new ListMethods(this);
      _methodHelp = new MethodHelp(this);
    } else {
      addMethod(_listMethods);
      addMethod(_methodHelp);
    }
  }
  else
  {
    removeMethod(LIST_METHODS);
    removeMethod(METHOD_HELP);
  }
}


void
XmlRpcServer::listMethods(XmlRpcValue& result)
{
  int i = 0;
  result.setSize(_methods.size()+1);
  for (MethodMap::iterator it=_methods.begin(); it != _methods.end(); ++it)
    result[i++] = it->first;

  // Multicall support is built into XmlRpcServer::executeRequest
  result[i] = MULTICALL;
}



// Parse the request, run the method, generate a response string.
std::string
XmlRpcServer::executeRequest(std::string const& request)
{
  XmlRpcValue params, resultValue;
  std::string methodName = parseRequest(request, params);
  XmlRpcUtil::log(2, "XmlRpcServer::executeRequest: server calling method '%s'", 
                    methodName.c_str());

  std::string response;
  try {

    if ( ! executeMethod(methodName, params, resultValue) &&
         ! executeMulticall(methodName, params, resultValue))
      response = generateFaultResponse(methodName + ": unknown method name");
    else
      response = generateResponse(resultValue.toXml());

  } catch (const XmlRpcException& fault) {
    XmlRpcUtil::log(2, "XmlRpcServer::executeRequest: fault %s.",
                    fault.getMessage().c_str()); 
    response = generateFaultResponse(fault.getMessage(), fault.getCode());
  }

  return response;
}

// Parse the method name and the argument values from the request.
std::string
XmlRpcServer::parseRequest(std::string const& request, XmlRpcValue& params)
{
  int offset = 0;   // Number of chars parsed from the request

  std::string methodName = XmlRpcUtil::parseTag(METHODNAME_TAG, request, &offset);

  if (methodName.size() > 0 && XmlRpcUtil::findTag(PARAMS_TAG, request, &offset))
  {
    int nArgs = 0;
    while (XmlRpcUtil::nextTagIs(PARAM_TAG, request, &offset)) {
      params[nArgs++] = XmlRpcValue(request, &offset);
      (void) XmlRpcUtil::nextTagIs(PARAM_ETAG, request, &offset);
    }

    (void) XmlRpcUtil::nextTagIs(PARAMS_ETAG, request, &offset);
  }

  return methodName;
}

// Execute a named method with the specified params.
bool
XmlRpcServer::executeMethod(const std::string& methodName, 
                            XmlRpcValue& params, 
                            XmlRpcValue& result)
{
  XmlRpcServerMethod* method = findMethod(methodName);

  if ( ! method) return false;

  method->execute(params, result);

  // Ensure a valid result value
  if ( ! result.valid())
      result = std::string();

  return true;
}

// Execute multiple calls and return the results in an array.
bool
XmlRpcServer::executeMulticall(const std::string& methodName, 
                               XmlRpcValue& params, 
                               XmlRpcValue& result)
{
  if (methodName != MULTICALL) return false;

  // There ought to be 1 parameter, an array of structs
  if (params.size() != 1 || params[0].getType() != XmlRpcValue::TypeArray)
    throw XmlRpcException(MULTICALL + ": Invalid argument (expected an array)");

  int nc = params[0].size();
  result.setSize(nc);

  for (int i=0; i<nc; ++i) {

    if ( ! params[0][i].hasMember(METHODNAME) ||
         ! params[0][i].hasMember(PARAMS)) {
      result[i][FAULTCODE] = -1;
      result[i][FAULTSTRING] = MULTICALL +
              ": Invalid argument (expected a struct with members methodName and params)";
      continue;
    }

    const std::string& methodName = params[0][i][METHODNAME];
    XmlRpcValue& methodParams = params[0][i][PARAMS];

    XmlRpcValue resultValue;
    resultValue.setSize(1);
    try {
      if ( ! executeMethod(methodName, methodParams, resultValue[0]) &&
           ! executeMulticall(methodName, params, resultValue[0]))
      {
        result[i][FAULTCODE] = -1;
        result[i][FAULTSTRING] = methodName + ": unknown method name";
      }
      else
        result[i] = resultValue;

    } catch (const XmlRpcException& fault) {
        result[i][FAULTCODE] = fault.getCode();
        result[i][FAULTSTRING] = fault.getMessage();
    }
  }

  return true;
}


// Create a response from results xml
std::string
XmlRpcServer::generateResponse(std::string const& resultXml)
{
  const char RESPONSE_1[] = 
    "<?xml version=\"1.0\"?>\r\n"
    "<methodResponse><params><param>\r\n\t";
  const char RESPONSE_2[] =
    "\r\n</param></params></methodResponse>\r\n";

  std::string body = RESPONSE_1 + resultXml + RESPONSE_2;
  std::string header = generateHeader(body);
  std::string response = header + body;

  XmlRpcUtil::log(5, "XmlRpcServer::generateResponse:\n%s\n", response.c_str());
  return response;
}


// Prepend http headers
std::string
XmlRpcServer::generateHeader(std::string const& body)
{
  std::string header = 
    "HTTP/1.1 200 OK\r\n"
    "Server: ";
  header += XMLRPC_VERSION;
  header += "\r\n"
    "Content-Type: text/xml\r\n"
    "Content-length: ";

  char buffLen[40];
  sprintf(buffLen,"%d\r\n\r\n", body.size());

  return header + buffLen;
}


std::string
XmlRpcServer::generateFaultResponse(std::string const& errorMsg, int errorCode)
{
  const char RESPONSE_1[] = 
    "<?xml version=\"1.0\"?>\r\n"
    "<methodResponse><fault>\r\n\t";
  const char RESPONSE_2[] =
    "\r\n</fault></methodResponse>\r\n";

  XmlRpcValue faultStruct;
  faultStruct[FAULTCODE] = errorCode;
  faultStruct[FAULTSTRING] = errorMsg;
  std::string body = RESPONSE_1 + faultStruct.toXml() + RESPONSE_2;
  std::string header = generateHeader(body);

  return header + body;
}

