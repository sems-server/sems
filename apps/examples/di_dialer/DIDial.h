#include "AmApi.h"

#include "ampi/UACAuthAPI.h"

#include <string>
using std::string;

struct DIDialoutInfo : UACAuthCred {
  string userpart; // user part for dialout
  DIDialoutInfo(const string& userpart, 
	      const string& realm,
	      const string& user,
	      const string& pwd) 
    : userpart(userpart), 
      UACAuthCred(realm, user, pwd) { }

  DIDialoutInfo() { }
};

class DIDial : public AmDynInvoke

{
  string dialout(const string& application, 
		 const string& user,
		 const string& from, 
		 const string& to,
		 AmArg* extra_params);

  string dialout_auth(const string& application, 
		      const string& user,
		      const string& from, 
		      const string& to,
		      const string& realm, 
		      const string& a_user,
		      const string& pwd,
		      AmArg* extra_params);

  string dialout_auth_b2b(const string& application, 
		       const string& announcement, 
		       const string& from, 
		       const string& to,
		       const string& caller_ruri, 
		       const string& callee_ruri,
		       const string& a_realm, 
		       const string& a_user, 
		       const string& a_pwd);

  string dialout_pin(const string& application, 
		     const string& user,
		     const string& to_user,
		     const string& pin);

  static DIDial* _instance;
 public:
  DIDial();
  ~DIDial();
  static DIDial* instance();
  void invoke(const string& method, const AmArg& args, AmArg& ret);
};
