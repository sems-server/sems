Readme for registrar_client component

This component registers on behalf of some application at a 
SIP registrar. Registrations are refreshed until they are 
explicitely removed via "removeRegistration". 

It is invoked via DI API, or used by the reg_agent module.

Loading uac_auth module is needed if the registration should
be authenticated!

API functions: 
=============

createRegistration
------------------
args: 
	   CStr domain    : domain for te registration  
	   CStr user      : user   """ 
	   CStr name      : display name   """
	   CStr auth_user : authentication user  """
	   CStr pwd       : password
	   CStr sess_link : local tag of session or name of factory that receives
                        events about the status of the registration
   optional:
           CStr proxy     : proxy to be used for registration
           CStr contact   : contact to register

returns: CStr handle  : used to remove the registration or correlate events

getRegistrationState
--------------------

return the state of a registration.

 args:
          CStr handle     : handle of the registration (as returned by createRegistration)

 returns: 
          int result      : 1 - OK
                            0 - registration does not exist 

       if result == 1: 
          int state       : state:     RegisterPending:  0 
                                       RegisterActive:   1
                                       RegisterActive:   2
          int expires     : expires left (seconds)

removeRegistration
------------------
args:  CStr handle    
returns: -

Events: 
======
SIPRegistrationEvent as in ampi/SIPRegistrarClientAPI.h


example code
============
	string domain, user, display_name, auth_user, passwd, sess_link;
	string handle; 
// assign some values 
    ...

//
	AmDynInvokeFactory* di_f = AmPlugIn::instance()->
           getFactory4Di("registrar_client");
	if (di_f == NULL) {
		DBG("unable to get a registrar_client\n");
	} else {
		AmDynInvoke* uac_auth_i = di_f->getInstance();
		if (uac_auth_i!=NULL) {
	        AmDynInvokeFactory* di_f = AmPlugIn::instance()->
                 getFactory4Di("registrar_client");

			AmArg di_args,ret;
			di_args.push(domain.c_str());
			di_args.push(user.c_str());
			di_args.push(display_name.c_str()); // display name
			di_args.push(auth_user.c_str());  // auth_user
			di_args.push(passwd.c_str());    // pwd
			di_args.push(sess_link.c_str()); //
			
			uac_auth_i->invoke("createRegistration", di_args, ret);

			handle = ret.get(0).asCStr();
		}
	}

