#include "CCAcc.h"

#include "AmSessionContainer.h"
#include "AmPlugIn.h"
#include "log.h"
#include "AmConfigReader.h"
#include "AmUtils.h"
#include "AmArg.h"

#include "XmlRpc.h"
#include "XmlRpcClient.h"
using namespace XmlRpc;


class CCAccFactory : public AmDynInvokeFactory
{
public:
     CCAccFactory(const string& name)
	: AmDynInvokeFactory(name) {}

     AmDynInvoke* getInstance(){
	return CCAcc::instance();
     }

     int onLoad(){
       DBG("CCAcc calling card accounting loaded.\n");
       return 0;
     }
};

EXPORT_PLUGIN_CLASS_FACTORY(CCAccFactory,"cc_acc");

CCAcc* CCAcc::_instance=0;

CCAcc* CCAcc::instance()
{
     if(!_instance)
	_instance = new CCAcc();
     return _instance;
}

CCAcc::CCAcc() {
}

CCAcc::~CCAcc() { }

void CCAcc::invoke(const string& method, const AmArg& args, AmArg& ret)
{
     if(method == "getCredit"){
       ret.push(getCredit(args.get(0).asCStr()));
     } else if(method == "subtractCredit"){
       ret.push(subtractCredit(args.get(0).asCStr(),
			      args.get(1).asInt()));	
     } else if(method == "connectCall"){
       //
     } else if(method == "_list"){
       ret.push("getCredit");
       ret.push("subtractCredit");
       ret.push("connectCall");
     }
     else
	throw AmDynInvoke::NotImplemented(method);
}

/* accounting functions... */;
int CCAcc::getCredit(string pin) {	
   const char* serverAddress;
   int port;
   serverAddress = "localhost";
   const char* uri = 0;
   port = 8000;
   XmlRpcClient xmlrpccall(serverAddress, port, uri, false);
   XmlRpcValue noArgs, result;
   XmlRpcValue xmlArg;
   xmlArg[0] = pin;
   xmlrpccall.execute("getCredit", xmlArg, result);
   int res = result;
   DBG("Credit Left '%u' .\n", res);
   return res;
}

int CCAcc::subtractCredit(string pin, int amount) {
   const char* serverAddress;
   int port;
   serverAddress = "localhost";
   const char* uri = 0;
   port = 8000;
   XmlRpcClient xmlrpccall(serverAddress, port, uri, false);
   XmlRpcValue noArgs, result;
   XmlRpcValue xmlArg;
   xmlArg[0][0]["methodName"] = "subtractCredit";
   xmlArg[0][0]["pin"] = pin;
   xmlArg[0][0]["amount"] = amount;	
   DBG("subtractCredit pin# '%s', Seconds '%u'.\n", pin.c_str(),  
amount );
   xmlrpccall.execute("subtractCredit", xmlArg, result);
   int res = result;
   DBG("Credit Left '%u' .\n", res);
   return res;
}
