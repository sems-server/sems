#ifndef _XMLRPCSOCKET_H_
#define _XMLRPCSOCKET_H_
//
// XmlRpc++ Copyright (c) 2002-2003 by Chris Morley
//
#if defined(_MSC_VER)
# pragma warning(disable:4786)    // identifier was truncated in debug info
#endif

#ifndef MAKEDEPEND
# include <string>
#endif

#include <openssl/ssl.h>

namespace XmlRpc {

  //! A platform-independent socket API.
  class XmlRpcSocket {
  public:
    // TODO - typedef the socket type, casting to int won't work for 64 bit windows

    //! Creates a stream (TCP) socket. Returns -1 on failure.
    static int socket();

    //! Closes a socket.
    static void close(int socket);


    //! Sets a stream (TCP) socket to perform non-blocking IO. Returns false on failure.
    static bool setNonBlocking(int socket);

    //! Read text from the specified socket. Returns false on error.
    static bool nbRead(int socket, std::string& s, bool *eof, SSL *ssl);

    //! Write text to the specified socket. Returns false on error.
    static bool nbWrite(int socket, std::string& s, int *bytesSoFar, SSL* ssl);


    // The next four methods are appropriate for servers.

    //! Allow the port the specified socket is bound to to be re-bound immediately so 
    //! server re-starts are not delayed. Returns false on failure.
    static bool setReuseAddr(int socket);

    //! Bind to a specified port
    static bool bind(int socket, int port);

    //! Set socket in listen mode
    static bool listen(int socket, int backlog);

    //! Accept a client connection request
    static int accept(int socket);

    //! Connect a socket to a server (from a client)
    static bool connect(int socket, std::string& host, int port);

    //! Get the port of a bound socket
    static int getPort(int socket);

    //! Returns true if the last error was not a fatal one (eg, EWOULDBLOCK)
    static bool nonFatalError();

    //! Returns last errno
    static int getError();

    //! Returns message corresponding to last error
    static std::string getErrorMsg();

    //! Returns message corresponding to error
    static std::string getErrorMsg(int error);
  };

} // namespace XmlRpc

#endif
