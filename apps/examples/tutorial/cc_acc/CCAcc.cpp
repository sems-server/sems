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
  // add some sample credits
  credits["12345"] = 100;
}

CCAcc::~CCAcc() { }

void CCAcc::invoke(const string& method, const AmArg& args, AmArg& ret)
{
    if(method == "getCredit"){
      ret.push(getCredit(args.get(0).asCStr()));
    }
    else if(method == "subtractCredit"){
      ret.push(subtractCredit(args.get(0).asCStr(),
			      args.get(1).asInt()));	
    }
    else
	throw AmDynInvoke::NotImplemented(method);
}

/* accounting functions... */
int CCAcc::getCredit(string pin) {
  credits_mut.lock();
  map<string, unsigned int>::iterator it =  credits.find(pin);
  if (it == credits.end()) {
    DBG("PIN '%s' dies not exist.", pin.c_str());
    credits_mut.unlock();
    return -1;
  }
  unsigned int res = it->second;
  credits_mut.unlock();
  return res;
}

int CCAcc::subtractCredit(string pin, int amount) {
  credits_mut.lock();
  map<string, unsigned int>::iterator it =  credits.find(pin);
  if (it == credits.end()) {
    ERROR("PIN '%s' dies not exist.", pin.c_str());
    credits_mut.unlock();
    return -1;
  }
  if (it->second - amount < 0) {
    credits[pin] = 0;
  } else {
    credits[pin] = it->second - amount;
  }
  unsigned int res =  credits[pin];
  credits_mut.unlock();
  return res;
}

