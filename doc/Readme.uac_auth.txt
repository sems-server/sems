uac_auth Client / Server authentication 

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


How to use server auth
----------------------

To authenticate a request, use the "checkAuth" DI function.

 Arguments:
     args[0] - ArgObject pointing to the Request (AmSipRequest object)
     args[1] - realm
     args[2] - user
     args[3] - password
 Return values:
     ret[0] - code: 200 for successful auth, 401 for unsuccessful (ie.
     ret[1] - reason string
     ret[2] - optional auth header

Limitations
-----------

- URI in auth hdr is not checked
- multiple Authorization headers are probably not properly processed (only the first one is used)

code example:

    AmDynInvokeFactory* fact = 
      AmPlugIn::instance()->getFactory4Di("uac_auth");
    if (NULL != fact) {
      AmDynInvoke* di_inst = fact->getInstance();
      if(di_inst) {
	AmArg di_args, di_ret;
	try {
	  di_args.push(AmArg((AmObject*)&req));
	  di_args.push("myrealm");
	  di_args.push("myuser");
	  di_args.push("mypwd");
	  di_inst->invoke("checkAuth", di_args, di_ret);
	  
	  if (di_ret.size() >= 3) {
	    if (di_ret[0].asInt() != 200) {
	      DBG("Auth: replying %u %s - hdrs: '%s'\n",
		  di_ret[0].asInt(), di_ret[1].asCStr(), di_ret[2].asCStr());
	      dlg->reply(req, di_ret[0].asInt(), di_ret[1].asCStr(), NULL, di_ret[2].asCStr());
	      return;
	    } else {
	      DBG("Successfully authenticated request.\n");
	    }
	  }
	} catch (const AmDynInvoke::NotImplemented& ni) {
	  ERROR("not implemented DI function 'checkAuth'\n");
	} catch (const AmArg::OutOfBoundsException& oob) {
	  ERROR("out of bounds in  DI call 'checkAuth'\n");
	} catch (const AmArg::TypeMismatchException& oob) {
	  ERROR("type mismatch  in  DI call checkAuth\n");
	} catch (...) {
	  ERROR("unexpected Exception  in  DI call checkAuth\n");
	}
     }
  }
