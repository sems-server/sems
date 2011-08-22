* mod_dlg saves the initial INVITE to DSMSession::last_req,
  this can be processed with dlg.reply(...)/dlg.acceptInvite(...)
* set connect_session to 0 with set(connect_session=0)
  if you want to reply with other than the standard 200 OK 
  to initial INVITE received.
* for processing of other requests, use enable_request_events and replyRequest

dlg.reply(code,reason);
 reply to the request in DSMSession::last_req 
 (usually INVITE, if not yet replied) with code and reason
 * sets $errno (arg,general)

dlg.replyRequest(code,reason);
 request processing in script; use with set($enable_request_events="true");
 reply to any request (in avar[DSM_AVAR_REQUEST]) with code and reason
 * sets $errno (arg,general)
 * throws exception if request not found (i.e. called from other event than
   sipRequest)

dlg.acceptInvite([code, reason]);
 e.g. dlg.acceptInvite(183, progress);
 * sets $errno (arg,general)
 
 accept audio stream from last_req (INVITE), and reply with 200 OK (default)
 or code, reason
 
dlg.bye([headers])
 send BYE. useful for example for continuing processing after call has ended.
 * sets $errno (general)

dlg.connectCalleeRelayed(string remote_party, string remote_uri)
 like B2B.connectCallee() but for relayed INVITEs, i.e. for executing in 
 invite run (run_invite_event=yes and transition "on INVITE" START - invite -> runinvite;)


dlg.dialout(string arrayname)
  dial out a new call
  simple format/mandatory:
   arrayname_caller   caller user
   arrayname_callee   callee user
   arrayname_domain   domain caller and callee
   arrayname_app      application to execute

  additional/overwrite:
   arrayname_r_uri     INVITE request URI
   arrayname_from      From
   arrayname_from_uri  From URI (only internally used)
   arrayname_to        To
   arrayname_auth_user authentication user
   arrayname_auth_pwd  authentication pwd
   arrayname_ltag      ltag for new call - must be new
   arrayname_hdrs      headers for new call

   arrayname_var.*     variables for new call, e.g.
                       arrayname_var.somevar will be set as $somevar

  returns $arrayname_ltag (if successful) and sets ERRNO.
   



