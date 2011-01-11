
#ifndef _XMLRPCSERVER_H_
#define _XMLRPCSERVER_H_
//
// XmlRpc++ Copyright (c) 2002-2003 by Chris Morley
//
#if defined(_MSC_VER)
# pragma warning(disable:4786)    // identifier was truncated in debug info
#endif

#ifndef MAKEDEPEND
# include <map>
# include <string>
#endif

#include "XmlRpcDispatch.h"
#include "XmlRpcSource.h"

namespace XmlRpc {


  // An abstract class supporting XML RPC methods
  class XmlRpcServerMethod;

  // Class representing connections to specific clients
  class XmlRpcServerConnection;

  // Class representing argument and result values
  class XmlRpcValue;


  //! A class to handle XML RPC requests
  class XmlRpcServer : public XmlRpcSource {
  public:
    //! Create a server object.
    XmlRpcServer();
    //! Destructor.
    virtual ~XmlRpcServer();

    //! Specify whether introspection is enabled or not. Default is not enabled.
    void enableIntrospection(bool enabled=true);

    //! Add a command to the RPC server
    void addMethod(XmlRpcServerMethod* method);

    //! Remove a command from the RPC server
    void removeMethod(XmlRpcServerMethod* method);

    //! Remove a command from the RPC server by name
    void removeMethod(const std::string& methodName);

    //! Look up a method by name
    XmlRpcServerMethod* findMethod(const std::string& name) const;

    //! Create a socket, bind to the specified port, and
    //! set it in listen mode to make it available for clients.
    //! @param port The port to bind and listen on (zero to choose an arbitrary port)
    //! @param bind_ip The IP to bind and listen on ("" to listen on ANY interface)
    bool bindAndListen(int port, const std::string& bind_ip, int backlog = 5);

    //! Get the port number this server is listening on.
    int getPort(void) const;

    //! Process client requests for the specified time
    void work(double msTime);

    //! Temporarily stop processing client requests and exit the work() method.
    void exit();

    //! Close all connections with clients and the socket file descriptor
    void shutdown();

    //! Introspection support
    void listMethods(XmlRpcValue& result);


    //! Parses the request xml, runs the method, generates the response (header+xml).
    //! Returns a fault response if an error occurs during method execution.
    virtual std::string executeRequest(std::string const& request);


    // XmlRpcSource interface implementation

    //! Handle client connection requests
    virtual unsigned handleEvent(unsigned eventType);

    //! Remove a connection from the dispatcher
    virtual void removeConnection(XmlRpcServerConnection*);

  protected:

    // Static data
    static const char METHODNAME_TAG[];
    static const char PARAMS_TAG[];
    static const char PARAMS_ETAG[];
    static const char PARAM_TAG[];
    static const char PARAM_ETAG[];

    static const std::string SYSTEM_MULTICALL;
    static const std::string METHODNAME;
    static const std::string PARAMS;

    static const std::string FAULTCODE;
    static const std::string FAULTSTRING;


    //! Accept a client connection request
    virtual void acceptConnection();

    //! Create a new connection object for processing requests from a specific client.
    //! If the client is not authorized to connect, close the socket and return 0.
    virtual XmlRpcServerConnection* createConnection(int socket);

    //! Hand off a new connection object to a dispatcher.
    virtual void dispatchConnection(XmlRpcServerConnection* sc);


    //! Parse the methodName and parameters from the request.
    //! @returns the methodName
    std::string parseRequest(std::string const& request, XmlRpcValue& params);

    //! Execute a named method with the specified params.
    bool executeMethod(const std::string& methodName, XmlRpcValue& params, XmlRpcValue& result);

    //! Execute multiple calls and return the results in an array.
    //! System.multicall implementation
    bool executeMulticall(const std::string& methodName, XmlRpcValue& params, XmlRpcValue& result);

    //! Construct a response from the result XML.
    std::string generateResponse(std::string const& resultXml);

    //! Construct a fault response.
    std::string generateFaultResponse(std::string const& msg, int errorCode = -1);

    //! Return the appropriate headers for the response.
    std::string generateHeader(std::string const& body);


    
    //! Whether the introspection API is supported by this server
    bool _introspectionEnabled;

    //! Event dispatcher
    XmlRpcDispatch _disp;

    //! Collection of methods. This could be a set keyed on method name if we wanted...
    typedef std::map< std::string, XmlRpcServerMethod* > MethodMap;

    //! Registered RPC methods.
    MethodMap _methods;

    //! List all registered RPC methods (only available if introspection is enabled)
    XmlRpcServerMethod* _listMethods;

    //! Return help string for a specified method (only available if introspection is enabled)
    XmlRpcServerMethod* _methodHelp;

  };
} // namespace XmlRpc

#endif //_XMLRPCSERVER_H_
