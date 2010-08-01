
#include "XmlRpcServerConnection.h"

#include "XmlRpcSocket.h"
#ifndef MAKEDEPEND
# include <stdio.h>
# include <stdlib.h>
#endif

#include "XmlRpcDispatch.h"
#include "XmlRpcServer.h"
#include "XmlRpcUtil.h"

#include <string.h>
#include <strings.h>

using namespace XmlRpc;



// The server delegates handling client requests to a serverConnection object.
XmlRpcServerConnection::XmlRpcServerConnection(int fd,
                                               XmlRpcServer* server,
                                               bool deleteOnClose /*= false*/) :
  XmlRpcSource(fd, deleteOnClose)
{
  XmlRpcUtil::log(2,"XmlRpcServerConnection: new socket %d.", fd);
  _server = server;
  _connectionState = READ_HEADER;
  _keepAlive = true;
}


XmlRpcServerConnection::~XmlRpcServerConnection()
{
  XmlRpcUtil::log(4,"XmlRpcServerConnection dtor.");
  _server->removeConnection(this);
}


// Handle input on the server socket by accepting the connection
// and reading the rpc request. Return true to continue to monitor
// the socket for events, false to remove it from the dispatcher.
unsigned
XmlRpcServerConnection::handleEvent(unsigned /*eventType*/)
{
  if (_connectionState == READ_HEADER)
    if ( ! readHeader()) return 0;

  if (_connectionState == READ_REQUEST)
    if ( ! readRequest()) return 0;

  if (_connectionState == WRITE_RESPONSE)
    if ( ! writeResponse()) return 0;

  return (_connectionState == WRITE_RESPONSE) 
        ? XmlRpcDispatch::WritableEvent : XmlRpcDispatch::ReadableEvent;
}


bool
XmlRpcServerConnection::readHeader()
{
  // Read available data
  bool eof;
  if ( ! XmlRpcSocket::nbRead(this->getfd(), _header, &eof, _ssl_ssl)) {
    // Its only an error if we already have read some data
    if (_header.length() > 0)
      XmlRpcUtil::error("XmlRpcServerConnection::readHeader: error while reading header (%s).",XmlRpcSocket::getErrorMsg().c_str());
    return false;
  }

  XmlRpcUtil::log(4, "XmlRpcServerConnection::readHeader: read %d bytes.", _header.length());
  char *hp = (char*)_header.c_str();  // Start of header
  char *ep = hp + _header.length();   // End of string
  char *bp = 0;                       // Start of body
  char *lp = 0;                       // Start of content-length value
  char *kp = 0;                       // Start of connection value

  for (char *cp = hp; (bp == 0) && (cp < ep); ++cp) {
	if ((ep - cp > 16) && (strncasecmp(cp, "Content-length: ", 16) == 0))
	  lp = cp + 16;
	else if ((ep - cp > 12) && (strncasecmp(cp, "Connection: ", 12) == 0))
	  kp = cp + 12;
	else if ((ep - cp > 4) && (strncmp(cp, "\r\n\r\n", 4) == 0))
	  bp = cp + 4;
	else if ((ep - cp > 2) && (strncmp(cp, "\n\n", 2) == 0))
	  bp = cp + 2;
  }

  // If we haven't gotten the entire header yet, return (keep reading)
  if (bp == 0) {
    // EOF in the middle of a request is an error, otherwise its ok
    if (eof) {
      XmlRpcUtil::log(4, "XmlRpcServerConnection::readHeader: EOF");
      if (_header.length() > 0)
        XmlRpcUtil::error("XmlRpcServerConnection::readHeader: EOF while reading header");
      return false;   // Either way we close the connection
    }
    
    return true;  // Keep reading
  }

  // Decode content length
  if (lp == 0) {
    XmlRpcUtil::error("XmlRpcServerConnection::readHeader: No Content-length specified");
    return false;   // We could try to figure it out by parsing as we read, but for now...
  }

  _contentLength = atoi(lp);
  if (_contentLength <= 0) {
    XmlRpcUtil::error("XmlRpcServerConnection::readHeader: Invalid Content-length specified (%d).", _contentLength);
    return false;
  }
  	
  XmlRpcUtil::log(3, "XmlRpcServerConnection::readHeader: specified content length is %d.", _contentLength);

  // Otherwise copy non-header data to request buffer and set state to read request.
  _request = bp;

  // Parse out any interesting bits from the header (HTTP version, connection)
  _keepAlive = true;
  if (_header.find("HTTP/1.0") != std::string::npos) {
    if (kp == 0 || strncasecmp(kp, "keep-alive", 10) != 0)
      _keepAlive = false;           // Default for HTTP 1.0 is to close the connection
  } else {
    if (kp != 0 && strncasecmp(kp, "close", 5) == 0)
      _keepAlive = false;
  }
  XmlRpcUtil::log(3, "KeepAlive: %d", _keepAlive);


  _header = ""; 
  _connectionState = READ_REQUEST;
  return true;    // Continue monitoring this source
}



bool
XmlRpcServerConnection::readRequest()
{
  // If we dont have the entire request yet, read available data
  if (int(_request.length()) < _contentLength) {
    bool eof;
    if ( ! XmlRpcSocket::nbRead(this->getfd(), _request, &eof, _ssl_ssl)) {
      XmlRpcUtil::error("XmlRpcServerConnection::readRequest: read error (%s).",XmlRpcSocket::getErrorMsg().c_str());
      return false;
    }

    // If we haven't gotten the entire request yet, return (keep reading)
    if (int(_request.length()) < _contentLength) {
      if (eof) {
        XmlRpcUtil::error("XmlRpcServerConnection::readRequest: EOF while reading request");
        return false;   // Either way we close the connection
      }
      return true;
    }
  }

  // Otherwise, parse and dispatch the request
  XmlRpcUtil::log(3, "XmlRpcServerConnection::readRequest read %d bytes.", _request.length());
  //XmlRpcUtil::log(5, "XmlRpcServerConnection::readRequest:\n%s\n", _request.c_str());

  _connectionState = WRITE_RESPONSE;

  return true;    // Continue monitoring this source
}



bool
XmlRpcServerConnection::writeResponse()
{
  if (_response.length() == 0) {
    executeRequest();
    _bytesWritten = 0;
    if (_response.length() == 0) {
      XmlRpcUtil::error("XmlRpcServerConnection::writeResponse: empty response.");
      return false;
    }
  }

  // Try to write the response
  if ( ! XmlRpcSocket::nbWrite(this->getfd(), _response, &_bytesWritten, _ssl_ssl)) {
    XmlRpcUtil::error("XmlRpcServerConnection::writeResponse: write error (%s).",XmlRpcSocket::getErrorMsg().c_str());
    return false;
  }
  XmlRpcUtil::log(3, "XmlRpcServerConnection::writeResponse: wrote %d of %d bytes.", _bytesWritten, _response.length());

  // Prepare to read the next request
  if (_bytesWritten == int(_response.length())) {
    _header = "";
    _request = "";
    _response = "";
    _connectionState = READ_HEADER;
  } else {
    return true; // not whole reponse written - continue in WRITE_RESPONSE state
  }

  return _keepAlive;    // Continue monitoring this source if true
}


//! Helper method to execute the client request
void XmlRpcServerConnection::executeRequest()
{
  _response = _server->executeRequest(_request);
}

