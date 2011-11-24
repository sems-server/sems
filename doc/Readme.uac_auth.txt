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

  #include "AmUACAuth.h"
  ...

  class MyCall : public AmSession,
	  	    public CredentialHolder

 * set credentials on session creation
	AnnouncementDialog::AnnouncementDialog(...) 
    : CredentialHolder("myrealm", "myuser", "mypwd")


2) enable uac_auth (add SessionEventHandler to session)
  MyCall* my_call=new MyCall();
  AmUACAuth::enable(my_call);

see announce_auth app for example application

