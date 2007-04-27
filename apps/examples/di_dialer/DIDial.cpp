#include "AmPlugIn.h"
#include "log.h"
#include "AmUAC.h"


#include "DIDial.h"

class DIDialFactory : public AmDynInvokeFactory
{
public:
    DIDialFactory(const string& name)
	: AmDynInvokeFactory(name) {}

    AmDynInvoke* getInstance(){
	return DIDial::instance();
    }

    int onLoad(){
      DBG("DIDial calling card accounting loaded.\n");
      return 0;
    }
};

EXPORT_PLUGIN_CLASS_FACTORY(DIDialFactory,"di_dial");

DIDial* DIDial::_instance=0;

DIDial* DIDial::instance()
{
    if(!_instance)
	_instance = new DIDial();
    return _instance;
}

DIDial::DIDial() {
}

DIDial::~DIDial() { }

void DIDial::invoke(const string& method, const AmArgArray& args, AmArgArray& ret)
{
    if(method == "dial"){
       ret.push(dialout(args.get(0).asCStr(), 
			args.get(1).asCStr(), 
			args.get(2).asCStr(), 
			args.get(3).asCStr()).c_str());
    } else if(method == "help"){
       ret.push("dial <application> <user> <from> <to>\n");
    } else
	throw AmDynInvoke::NotImplemented(method);
}

string DIDial::dialout(const string& application, 
		       const string& user, 
		       const string& from, 
		       const string& to) {
  DBG("dialout application '%s', user '%s', from '%s', to '%s'", 
      application.c_str(), user.c_str(), from.c_str(), to.c_str());

  AmSession* s = AmUAC::dialout(user.c_str(), application,  to,  
				"<" + from +  ">", from, "<" + to + ">");
  if (s)
    return s->getLocalTag();
  else 
    return "<failed>";
  
}
