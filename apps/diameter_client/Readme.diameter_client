
diameter client                   (C) 2007 iptego GmbH
---------------

this is a very simple DIAMETER client implementation. it does 
implement only parts of the base protocol, and is not a complete 
DIAMETER implementation.

it is used from other modules with the DI API - i.e. other modules 
can execute DI functions to add a server connection, or send a 
DIAMETER request.

the DIAMETER base implementation is based on ser-0.9.6 diameter_auth 
module by Elena-Ramona Modroiu,  Copyright © 2003, 2004 FhG FOKUS.
connection pool and asynchronous message handling has been added, 
together with the CER/CEA handshake and other things.

be prepared that you will need to look into the source to see how this 
works, and you will need to build up your request AVPs by hand by 
assembling an AmArg with the AVPs as ArgBlobs, and also unpack the 
results/a received event from an AmArg array yourself.

WHY?
----
It seems that there is no simple to use free DIAMETER client implementation
available. 
 - OpenDIAMETER is probably a complete solution, but seems to 
be very complex to use. 
 - DISC  (http://developer.berlios.de/projects/disc/) is of 2003, seems to 
   be quite complete but assumes to be a complete server
 - openimscore cdp module would probably have been a better basis 
   than ser 0.9.6's auth diameter. well...

BUGS/TODO
---------
 o CEA needs probably be fixed to specific AVP set
 o mandatory AVP checking in compund AVP not implemented
 

API
---
o connections are added with new_connection; connections are pooled for 
  an application, and are retried periodically if broken. only active 
  connections are used when sending a request (obviously)

o replies to requests are posted as event to the session/module 
  (identified by sess_link)

new_connection
  string app_name
  string server_ip
  unsigned int server_port
  string origin_host
  string origin_realm
  string origin_ip
  unsigned int app_id
  unsigned int vendor_id
  string product_name

send_request
  string app_name
  unsigned int command_code
  unsigned int app_id
  arg val
  string sess_link

  args:  array
    [int avp_id, int flags, int vendor, blob data]

 returns :
   DIA_OK
   DIA_ERR_NOAPP
   DIA_ERR_NOCONN

reply events : 
  ... 


some testing code
----------------

else if(method == "test1"){
    AmArg a; 
    a.push(AmArg("vtm"));
    a.push(AmArg("10.1.0.196"));
    a.push(AmArg(8080));
    a.push(AmArg("vtm01"));
    a.push(AmArg("vtm.t-online.de"));
    a.push(AmArg("10.42.32.13"));
    a.push(AmArg(16777241));
    a.push(AmArg(29631));
    a.push(AmArg("vtm"));
    a.assertArrayFmt("ssisssiis");
    newConnection(a, ret);
  } else if(method == "test2"){
    AmArg a; 
#define AAA_APP_USPI    16777241
#define AVP_E164_NUMBER     1024
#define AAA_VENDOR_IPTEGO  29631
#define AAA_LAR         16777214

    a.push(AmArg("vtm"));
    a.push(AmArg(AAA_LAR));
    a.push(AmArg(AAA_APP_USPI));
    DBG("x pushin \n");
    AmArg avps;

    AmArg e164;
    e164.push((int)AVP_E164_NUMBER);
    e164.push((int)AAA_AVP_FLAG_VENDOR_SPECIFIC | AAA_AVP_FLAG_MANDATORY);
    e164.push((int)AAA_VENDOR_IPTEGO);
    string e164_number = "+49331600001";
    e164.push(ArgBlob(e164_number.c_str(), e164_number.length()));
    avps.push(e164);

    AmArg drealm;
    drealm.push((int)AVP_Destination_Realm);
    drealm.push((int)AAA_AVP_FLAG_MANDATORY);
    drealm.push((int)0);
    string dest_realm = "iptego.de";
    drealm.push(ArgBlob(dest_realm.c_str(), dest_realm.length()));
    avps.push(drealm);

    a.push(avps);
    a.push(AmArg("bogus_link"));

    // check...
    a.assertArrayFmt("siias");

    // check values
    AmArg& vals = a.get(3);
    for (size_t i=0;i<vals.size(); i++) {
      AmArg& row = vals.get(i);
      //    [int avp_id, int flags, int vendor, blob data]
      row.assertArrayFmt("iiib");
    }
    DBG("x sendrequest\n");
    sendRequest(a, ret);

