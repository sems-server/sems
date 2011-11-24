
#include "AmUACAuth.h"

UACAuthCred::UACAuthCred() { }
UACAuthCred::UACAuthCred(const string& realm,
			 const string& user,
			 const string& pwd)
  : realm(realm), user(user), pwd(pwd) { }



AmUACAuth::AmUACAuth() { }

AmUACAuth::~AmUACAuth() { }


UACAuthCred* AmUACAuth::unpackCredentials(const AmArg& arg) {
  UACAuthCred* cred = NULL;
  if (arg.getType() == AmArg::AObject) {
    AmObject* cred_obj = arg.asObject();
    if (cred_obj)
      cred = dynamic_cast<UACAuthCred*>(cred_obj);
  }
  return cred;
}


bool AmUACAuth::enable(AmSession* s) {
  bool res = false;

  AmSessionEventHandlerFactory* uac_auth_f = 
    AmPlugIn::instance()->getFactory4Seh("uac_auth");
  if (uac_auth_f != NULL) {
    AmSessionEventHandler* h = uac_auth_f->getHandler(s);
    if (h != NULL ) {
      DBG("enabling SIP UAC auth for new session.\n");
      s->addHandler(h);
      res = true;
    } else {
      WARN("trying to get auth handler for invalid session "
	   "(derived from CredentialHolder?)\n");
    }
  } else {
    WARN("uac_auth interface not accessible. "
	  "Load uac_auth for authenticated calls.\n");
  }
  return res;
}
