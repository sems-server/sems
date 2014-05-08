* mod_dlg saves the initial INVITE to DSMSession::last_req,
  this can be processed with dlg.reply(...)/dlg.acceptInvite(...)
* set connect_session to 0 with set(connect_session=0)
  if you want to reply with other than the standard 200 OK 
  to initial INVITE received.
* for processing of other requests, use set($enable_request_events="true") and replyRequest
* for processing of other requests, use set($enable_reply_events="true") and replyRequest

* Request/Reply body handling with 
 dlg.requestHasContentType condition and dlg.getRequestBody action
 dlg.replyHasContentType condition and dlg.getReplyBody action

dlg.reply(code,reason);
 reply to the request in DSMSession::last_req 
 (usually INVITE, if not yet replied) with code and reason
 * sets $errno (arg,general)

dlg.replyRequest(code,reason);
 request processing in script; use with set($enable_request_events="true");
 reply to any request (in avar[DSM_AVAR_REQUEST]) with code and reason

 headers can be added by setting $dlg.reply.hdrs, e.g.
    set($dlg.reply.hdrs="P-Prompt-Type: Charging-info\\r\\nP-Prompt-Content: 5\\r\\n");
    dlg.acceptInvite(183, "progress");

 * sets $errno (arg,general)
 * throws exception if request not found (i.e. called from other event than
   sipRequest)

dlg.acceptInvite([code, reason]);
 e.g. dlg.acceptInvite(183, progress);
  headers can be added by setting $dlg.reply.hdrs, e.g.
    set($dlg.reply.hdrs="P-Prompt-Type: Charging-info\r\nP-Prompt-Content: 5\r\n");
    dlg.acceptInvite(183, "progress");
 * sets $errno (arg,general)
 
 accept audio stream from last_req (INVITE), and reply with 200 OK (default)
 or code, reason. sets "dlg" type errno if negotiation fails.
 
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

   
dlg.getOtherId(varname)
   get other related dlg id in $varname

dlg.getRtpRelayMode(varname)
   get RTP relay mode (RTP_Direct, RTP_Relay, RTP_Transcoding) in $varname

dlg.refer(string refer_to [, int expires=0])
   refer to refer_to, optionally with expires

dlg.info(content_type, body)
   send INFO request; use \r\n for crlf in body

dlg.relayError(code,reason);  -  relay reply (>=200) to B2B request (sbc)
  reply to B2B request (in avar[DSM_AVAR_REQUEST]) with code and reason
  sbc: set(#StopProcessing="true") to prevent B2B request to be relayed
       after replying from DSM script

Request/Reply Body handling in sipRequest/sipReply events:
----------------------------------------------------------
actions (applicable only in sipRequest/sipReply event handling blocks):
dlg.getRequestBody(content_type, dstvar)  - get body of content_type in $dstvar
dlg.getReplyBody(content_type, dstvar)    - get body of content_type in $dstvar
dlg.addReplyBodyPart(content_type, payload) - add new body part possibly
  converting the resulting body to multipart
dlg.deleteReplyBodyPart(content_type) - delete body part from multipart
  body possibly converting the resulting body to singlepart

conditions: 
  dlg.replyHasContentType(content_type) and dlg.requestHasContentType(content_type)

  checks whether request/reply has a certain content type

 example: 

transition "msg recvd" A - sipRequest; dlg.requestHasContentType(application/ISUP) / {
  dlg.getRequestBody(application/ISUP, isup_body);
  ... do sth with $isup_body ...
} -> B;
