* mod_dlg saves the initial INVITE to DSMSession::last_req


dlg.reply(code,reason);
 reply to the request in DSMSession::last_req 
 (usually INVITE, if not yet replied) with code and reason

dlg.acceptInvite([code, reason]);
 e.g. dlg.acceptInvite(183, progress);
 
 accept audio stream from last_req (INVITE), and reply with 200 OK (default)
 or code, reason
 
