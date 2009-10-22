uac_auth Client authentication 


how to use uac_auth
--------------------

Requests with 407/401 replies are authenticated and resent by the 
uac_auth SessionComponent plugin, if 
  1. Credentials are provided by the Session and 
  2. uac_auth is added as SessionEventHandler to the session 
    (so that the uac_auth gets incoming and outgoing messages).

How to
1) provide credentials
 * Derive your Session object from CredentialHolder:

  #include "../../core/plug-in/uac_auth/UACAuth.h"
  ...

  class AnnouncementDialog : public AmSession,
	  					     public CredentialHolder

 * set credentials on session creation
	AnnouncementDialog::AnnouncementDialog(...) 
    : CredentialHolder("myrealm", "myuser", "mypwd")


2) add uac_auth SessionEventHandler to session
	
 * get uac_auth factory interface (e.g. in onLoad):
    uac_auth_f = AmPlugIn::instance()->getFactory4Seh("uac_auth");
   
 * add when you create a new session:

    if (uac_auth_f != NULL) {
		DBG("UAC Auth enabled for new  session.\n");
		AmSessionEventHandler* h = uac_auth_f->getHandler(dlg);
		if (h != NULL )
			dlg->addHandler(h);
	}

see announce_auth app for example application

