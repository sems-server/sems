#ifndef _UACAuth_h_
#define _UACAuth_h_
#include "ampi/UACAuthAPI.h"
#include "AmSession.h"
#include "AmArg.h"

class AmUACAuth {
 protected:
  AmUACAuth();
  ~AmUACAuth();
 public:
  /**
     unpack UAC auth credentials from an AmArg 
     @return UACAuthCred object, NULL if failed 
  */

  static UACAuthCred* unpackCredentials(const AmArg& arg);

  /**
     enable SIP UAC authentication
     @return true if successful
  */
  static bool enable(AmSession* s);

};
#endif
