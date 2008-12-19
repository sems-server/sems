#ifndef _TO_XMLRPCCLIENT_H
#define _TO_XMLRPCCLIENT_H
#include "XmlRpcClient.h"
using namespace XmlRpc;

/* xmlrpc client with timeout */
class TOXmlRpcClient : public XmlRpc::XmlRpcClient {
 public:
  TOXmlRpcClient(const char* host, int port, const char* uri=0
#ifdef HAVE_XMLRPCPP_SSL
		 , bool ssl=false
#endif
		 )
    : XmlRpcClient(host, port, uri
#ifdef HAVE_XMLRPCPP_SSL
		   , ssl
#endif
		   ) { }

  bool execute(const char* method, XmlRpcValue const& params, 
	       XmlRpcValue& result, double timeout);
};

#endif
