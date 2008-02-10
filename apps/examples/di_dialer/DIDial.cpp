#include "DIDial.h"

#include "AmPlugIn.h"
#include "log.h"
#include "AmUAC.h"
#include "ampi/UACAuthAPI.h"
#include "AmConfigReader.h"
#include "AmUtils.h"

#include <map>

#define APP_NAME "di_dial"

class DIDialFactory : public AmDynInvokeFactory
{
public:
  DIDialFactory(const string& name)
    : AmDynInvokeFactory(name) {}
  
  AmDynInvoke* getInstance(){
    return DIDial::instance();
  }

  static std::map<string, DIDialoutInfo> dialout_pins;
  int onLoad();
};


// note its not really safe to store plaintext passwords in memory
std::map<string, DIDialoutInfo> DIDialFactory::dialout_pins;

int DIDialFactory::onLoad(){
  
  AmConfigReader cfg;
  if(!cfg.loadFile(AmConfig::ModConfigPath + string(APP_NAME)+ ".conf")) {

    unsigned int i_pin = 0;
    while (i_pin<100) { // only for safety..
      string dialout_pin = cfg.getParameter("dialout_pin"+int2str(i_pin));
      if (!dialout_pin.length()) 
	break;
      size_t pos = dialout_pin.find_first_of(';');
      if (pos == string::npos)
	break;
      string pin = dialout_pin.substr(0, pos);
      
      size_t pos2 = dialout_pin.find_first_of(';', pos+1);
      if ((pos == string::npos)||(pos2 == string::npos))
	break;
      string userpart = dialout_pin.substr(pos+1, pos2-pos-1);
      pos = pos2;

      pos2 = dialout_pin.find_first_of(';', pos+1);
      if ((pos == string::npos)||(pos2 == string::npos))
	break;
      string user = dialout_pin.substr(pos+1, pos2-pos-1);
      pos = pos2;

      pos2 = dialout_pin.find_first_of(';', pos+1);
      if ((pos == string::npos)||(pos2 == string::npos))
	break;
      string domain = dialout_pin.substr(pos+1, pos2-pos-1);
      pos = pos2;

      pos2 = dialout_pin.find_first_of(';', pos+1);
      if ((pos == string::npos)||(pos2 == string::npos))
	break;
      string pwd = dialout_pin.substr(pos+1, pos2-pos-1);
      pos = pos2;

      dialout_pins[pin] = DIDialoutInfo(userpart, domain, user, pwd);
      DBG("DIDial: added PIN '%s' userpart '%s' domain '%s' user '%s' pwd '<not shown>'\n", 
	  pin.c_str(), userpart.c_str(), domain.c_str(), user.c_str());
      i_pin++;		
    } 
  } else {
    DBG("no configuration for di_dial found. No dialout pins will be set.\n");
  }
  
  DBG("DIDial loaded.\n");
  return 0;
}

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

void DIDial::invoke(const string& method, const AmArg& args, AmArg& ret)
{
    if(method == "dial"){
       ret.push(dialout(args.get(0).asCStr(), 
			args.get(1).asCStr(), 
			args.get(2).asCStr(), 
			args.get(3).asCStr()).c_str());
    } else if(method == "dial_auth"){
       ret.push(dialout_auth(args.get(0).asCStr(), 
			args.get(1).asCStr(), 
			args.get(2).asCStr(), 
			args.get(3).asCStr(),
			args.get(4).asCStr(), 
			args.get(5).asCStr(), 
			args.get(6).asCStr()
			).c_str());
    } else if(method == "dial_auth_b2b"){
       ret.push(dialout_auth_b2b(args.get(0).asCStr(), 
      args.get(1).asCStr(), 
      args.get(2).asCStr(), 
      args.get(3).asCStr(),
      args.get(4).asCStr(), 
      args.get(5).asCStr(), 
      args.get(6).asCStr(),
      args.get(7).asCStr(),
      args.get(8).asCStr()
      ).c_str());
    } else if(method == "dial_pin"){
       ret.push(dialout_pin(args.get(0).asCStr(), 
			    args.get(1).asCStr(), 
			    args.get(2).asCStr(), 
			    args.get(3).asCStr()
			).c_str());
    } else if(method == "help"){
      ret.push("dial <application> <user> <from> <to>");
      ret.push("dial_auth <application> <user> <from> <to> <realm> <auth_user> <auth_pwd>");
      ret.push("dial_auth_b2b <application> <announcement> <from> <to> <caller_ruri> <callee_ruri> <realm> <auth_user> <auth_pwd>");
      ret.push("dial_pin <application> <dialout pin> <local_user> <to_user>");
    } else if(method == "_list"){ 
      ret.push(AmArg("dial"));
      ret.push(AmArg("dial_auth"));
      ret.push(AmArg("dial_auth_b2b"));
      ret.push(AmArg("dial_pin"));
      ret.push(AmArg("help"));
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

string DIDial::dialout_auth(const string& application, 
		       const string& user, 
		       const string& from, 
		       const string& to,
		       const string& a_realm, 
		       const string& a_user, 
		       const string& a_pwd
		       ) {
  DBG("dialout application '%s', user '%s', from '%s', to '%s', authrealm '%s', authuser '%s', authpass '%s'", 
      application.c_str(), user.c_str(), from.c_str(), to.c_str(), a_realm.c_str(), a_user.c_str(), a_pwd.c_str());

  AmArg* a = new AmArg();
  a->setBorrowedPointer(new UACAuthCred(a_realm, a_user, a_pwd));

  AmSession* s = AmUAC::dialout(user.c_str(), application,  to,  
				"<" + from +  ">", from, "<" + to + ">", 
				string(""), // callid
				string(""), // xtra hdrs
				a);
  if (s)
    return s->getLocalTag();
  else 
    return "<failed>\n";
}

string DIDial::dialout_auth_b2b(const string& application, 
          const string& announcement, 
          const string& from, 
          const string& to,
          const string& caller_ruri, 
          const string& callee_ruri,
          const string& a_realm, 
          const string& a_user, 
          const string& a_pwd
) {

  DBG("authenticated b2b dialout application '%s', from '%s', to '%s', caller '%s', callee '%s'\n",
    application.c_str(), from.c_str(), to.c_str(), caller_ruri.c_str(), callee_ruri.c_str());

  AmArg a;

  a.push(a_realm.c_str());
  a.push(a_user.c_str());
  a.push(a_pwd.c_str());
  a.push(callee_ruri.c_str());

  AmSession* s = AmUAC::dialout(
      announcement.c_str(), application,  caller_ruri,
      "<" + from +  ">", from, "<" + to + ">", 
      string(""), // callid
      string(""), //xtra hdrs
      &a);
  if (s)
    return s->getLocalTag();
  else 
    return "<failed>\n";
}

string DIDial::dialout_pin(const string& application, 
			   const string& pin,
			   const string& user, 
			   const string& to_user			   
		       ) {
  DBG("dialout application '%s', user '%s', to_user '%s', pin '%s'", 
      application.c_str(), user.c_str(), to_user.c_str(), pin.c_str());

    // find pin
  std::map<string, DIDialoutInfo>::iterator it = DIDialFactory::dialout_pins.find(pin);
  if (it != DIDialFactory::dialout_pins.end()) {
    AmArg* a = new AmArg();
    a->setBorrowedPointer(new UACAuthCred(it->second.realm, 
					  it->second.user, 
					  it->second.pwd));
    
    AmSession* s = AmUAC::dialout(user.c_str(), application,  
				  "sip:"+to_user+"@"+it->second.realm,  
				  "<sip:" + it->second.user+"@"+it->second.realm + ">", 
				  "sip:"+it->second.user+"@"+it->second.realm, 
				  "<sip:" + to_user+"@"+it->second.realm + ">", 
				  string(""), // callid
				  string(""), // xtra hdrs
				  a);
    if (s)
      return s->getLocalTag();
    else 
      return "<failed>\n";
  } else 
    return "incorrect dialout pin.\n";
}
