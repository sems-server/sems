#include "AmApi.h"

#include <string>
using std::string;

class DIDial : public AmDynInvoke

{
  string dialout(const string& application, 
		 const string& user,
		 const string& from, 
		 const string& to);

  static DIDial* _instance;
 public:
  DIDial();
  ~DIDial();
  static DIDial* instance();
  void invoke(const string& method, const AmArgArray& args, AmArgArray& ret);
};
