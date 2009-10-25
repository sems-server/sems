* mod_dlg saves the initial INVITE to DSMSession::last_req


dlg.reply(code,reason);
 reply to the request in DSMSession::last_req 
 (usually INVITE, if not yet replied) with code and reason
 * sets $errno (arg,general)

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
