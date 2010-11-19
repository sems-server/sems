SEMS pin_collect application Readme

This application collects a PIN and then transfers using a
(proprietary) REFER the call.

The authentication mode can be set in the configuration file 
(auth_mode parameter). 

Authentication Modes:
 XMLRPC : Authenticate against an XMLRPC server (python example 
          server in test/authserver.py

 REFER : The Refer-to of the REFER sent in-dialog contains <user>+<pin>@domain,
         such that this pin can be checked by an upstream app server or
         proxy and acted upon (e.g. sent to the proper conference room).

 TRANSFER : the transfer request (Transfer REFER) sent out has as user part of 
          the URI the original user part, a plus sign, and the entered 
          PIN. The PIN can thus be verified by the proxy handling the 
          transfer REFER. See below for an explanation.

"Transfer" REFER:
 The "Transfer REFER" is a proprietary REFER call flow which transfers a 
 SIP dialog and session to another user agent ('taker'). If the transfer 
 REFER is  accepted, the one transfering the call just "forgets" the dialog 
 and associated session, while the taker can send a re-Invite, thus overtaking
 the dialog and session. For this to work, both transferer and taker must
 be behind the same record routing proxy, and the callers user agent must 
 properly support re-Invite (updating of contact, and session, as specified 
 in RFC3261).

 The transfer request sent out has two session parameters (set in
 P-App-Param header), which are needed by the entity taking the call: 

  Transfer-RR  : route set of the call
  Transfer-NH  : next hop 
