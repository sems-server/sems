#include "AmPlugIn.h"
#include "log.h"

#include "CCAcc.h"

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
  // add some sample credits...
  // credits["12345"] = 100;
}

CCAcc::~CCAcc() { }

void CCAcc::invoke(const string& method, const AmArg& args, AmArg& ret)
{
  DBG("cc_acc: %s(%s)\n", method.c_str(), AmArg::print(args).c_str());

    if(method == "getCredit"){
      assertArgCStr(args.get(0));
      ret.push(getCredit(args.get(0).asCStr()));
    } else if(method == "subtractCredit"){
      assertArgCStr(args.get(0));
      assertArgInt(args.get(1));
      ret.push(subtractCredit(args.get(0).asCStr(),
			      args.get(1).asInt()));	
    } else if(method == "addCredit"){
      assertArgCStr(args.get(0));
      assertArgInt(args.get(1));
      ret.push(addCredit(args.get(0).asCStr(),
			 args.get(1).asInt()));	
    } else if(method == "setCredit"){
      assertArgCStr(args.get(0));
      assertArgInt(args.get(1));
      ret.push(setCredit(args.get(0).asCStr(),
			 args.get(1).asInt()));	
    } else if(method == "connectCall"){
      // call is connected
    } else if(method == "_list"){
      ret.push("getCredit");
      ret.push("subtractCredit");
      ret.push("setCredit");
      ret.push("addCredit");
      ret.push("connectCall");
    }
    else
	throw AmDynInvoke::NotImplemented(method);
}

/* accounting functions... */
int CCAcc::getCredit(string pin) {
  credits_mut.lock();
  std::map<string, unsigned int>::iterator it =  credits.find(pin);
  if (it == credits.end()) {
    DBG("PIN '%s' does not exist.\n", pin.c_str());
    credits_mut.unlock();
    return -1;
  }
  unsigned int res = it->second;
  credits_mut.unlock();
  return res;
}

int CCAcc::setCredit(string pin, int amount) {
  credits_mut.lock();
  credits[pin] = amount;
  credits_mut.unlock();
  return amount;
}

int CCAcc::addCredit(string pin, int amount) {
  credits_mut.lock();
  int res = (credits[pin] += amount);
  credits_mut.unlock();
  return res;
}

int CCAcc::subtractCredit(string pin, int amount) {
  credits_mut.lock();
  std::map<string, unsigned int>::iterator it =  credits.find(pin);
  if (it == credits.end()) {
    ERROR("PIN '%s' dies not exist.", pin.c_str());
    credits_mut.unlock();
    return -1;
  }
  if ((int)it->second - amount < 0) {
    credits[pin] = 0;
  } else {
    credits[pin] = it->second - amount;
  }
  unsigned int res =  credits[pin];
  credits_mut.unlock();
  return res;
}

