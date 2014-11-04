
ZRTP support module

(C) 2014 Stefan Sayer.  Parts of the development of this module was kindly sponsored by AMTEL Inc.

If SEMS is built with ZRTP support, a module mod_zrtp is created.

ZRTP may be individually enabled or disabled and configured in the DSM script by using
zrtp.setEnabled in the start event, for example
   import(mod_zrtp);
   initial state START;
   transition "start" START - start / {
    zrtp.setEnabled(false);
-- or, e.g.: zrtp.setAllowclear(false);
   } -> sess_started;
   state sess_started;

This overrides the sems.conf enable_zrtp setting. 

NOTE: The setEnabled/setAllowclear/... actions have only effect in the start event!

ZRTP specific events can be used in DSM if $enable_zrtp_events is set to true:
  set($enable_zrtp_events=true); 
Then the zrtp.protocolEvent and zrtp.securityEvent can be handled specifically.

Actions
-------
zrtp.setEnabled(bool enabled)
  e.g. zrtp.setEnabled(true) or zrtp.setEnabled(false)

zrtp.setAllowclear(bool enabled)  - set ZRTP allowclear option, see ZRTP documentation
zrtp.setAutosecure(bool enabled)  - set ZRTP autosecure option, see ZRTP documentation
zrtp.setDisclosebit(bool enabled) - set ZRTP disclose_bit option, see ZRTP documentation

zrtp.getSAS(sas1_var,sas2_var) - get SAS strings
 to be used after the stream has gone secure, e.g. 
  transition "ZRTP event IS_SECURE" sess_started - zrtp.protocolEvent(#event==ZRTP_EVENT_IS_SECURE) / {
     zrtp.getSAS($sas1,$sas2);
     log(3, $sas1);
     log(3, $sas2);
   } -> sess_started;

zrtp.getSessionInfo(varname) - get info about the session
  varname.sas_is_ready, varname.sas1, varname.sas2, varname.zid, varname.peer_zid, ...


zrtp.setVerified(string zid1, string zid2)   - set as verified
zrtp.setUnverified(string zid1, string zid2) - set as unverified

 e.g.  zrtp.getSessionInfo(zrtp);
       zrtp.setVerified($zrtp.zid,$zrtp.peer_zid);

Events
------

zrtp.protocolEvent - protocol event happened, only if enable_zrtp_events==true
 e.g. 
  zrtp.protocolEvent(#event==ZRTP_EVENT_IS_SECURE)

  #event    - event type
  #event_id - numeric event type (should not be used)
 
 see the libzrtp documentation, or /usr/include/libzrtp/zrtp_iface.h for
 event types. Important ones are
   ZRTP_EVENT_IS_SECURE, ZRTP_EVENT_NO_ZRTP, ZRTP_EVENT_IS_SECURE_DONE,
   ZRTP_EVENT_IS_CLEAR, ZRTP_EVENT_IS_PENDINGSECURE, 

  e.g. 
    transition "ZRTP event" sess_started - zrtp.protocolEvent / {
      logParams(3);
    } -> sess_started;

zrtp.securityEvent - security event happened, only if enable_zrtp_events==true
 e.g. 
  zrtp.securityEvent(#event==ZRTP_EVENT_PROTOCOL_ERROR)

  #event    - event type
  #event_id - numeric event type (should not be used)

  ZRTP_EVENT_PROTOCOL_ERROR, ZRTP_EVENT_WRONG_SIGNALING_HASH, ZRTP_EVENT_WRONG_MESSAGE_HMAC,
  ZRTP_EVENT_MITM_WARNING
       
