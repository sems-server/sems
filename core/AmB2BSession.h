#ifndef AmB2BSession_h
#define AmB2BSession_h

#include "AmSession.h"
#include "AmSipDialog.h"

enum { B2BTerminateLeg, 
       B2BConnectLeg, 
       B2BCallAccepted, 
       B2BSipRequest, 
       B2BSipReply };

struct B2BEvent: public AmEvent
{
    B2BEvent(int ev_id) 
	: AmEvent(ev_id)
    {}
};

struct B2BSipEvent: public B2BEvent
{
    bool forward;

    B2BSipEvent(int ev_id, bool forward)
	: B2BEvent(ev_id),
	  forward(forward)
    {}
};

struct B2BSipRequestEvent: public B2BSipEvent
{
    AmSipRequest req;

    B2BSipRequestEvent(const AmSipRequest& req, bool forward)
	: B2BSipEvent(B2BSipRequest,forward),
	  req(req)
    {}
};

struct B2BSipReplyEvent: public B2BSipEvent
{
    AmSipReply reply;

    B2BSipReplyEvent(const AmSipReply& reply, bool forward)
	: B2BSipEvent(B2BSipReply,forward),
	  reply(reply)
    {}
};

struct B2BConnectEvent: public B2BEvent
{
    string remote_party;
    string remote_uri;

    string content_type;
    string body;
    string hdrs;

    B2BConnectEvent(const string& remote_party,
		    const string& remote_uri)
	: B2BEvent(B2BConnectLeg),
	  remote_party(remote_party),
	  remote_uri(remote_uri)
    {}
};

class AmB2BSession: public AmSession
{
protected:
    // local tag of the other leg
    string other_id;

    // Tell if the session should
    // process SIP request itself
    // or only relay them.
    bool sip_relay_only;

    // Requests which
    // have been relayed (sent)
    TransMap relayed_req;

    // Requests received for relaying
    map<int,AmSipRequest> recvd_req;

    void clear_other();

    // Relay one event to the other side.
    virtual void relayEvent(AmEvent* ev);

    void relaySip(const AmSipRequest& req);
    void relaySip(const AmSipRequest& orig, const AmSipReply& reply);

    // Terminate our leg and forget the other.
    void terminateLeg();

    // Terminate the other leg and forget it.
    virtual void terminateOtherLeg();

    // @see AmSession
    void onSipRequest(const AmSipRequest& req);
    void onSipReply(const AmSipReply& reply);

    // @see AmEventQueue
    void process(AmEvent* event);

    // B2BEvent handler
    virtual void onB2BEvent(B2BEvent* ev);

    // Other leg received a BYE
    virtual void onOtherBye(const AmSipRequest& req);

    // INVITE from other leg has been replied
    virtual void onOtherReply(const AmSipReply& reply);

    AmB2BSession()
	: sip_relay_only(true)
    {}
    AmB2BSession(const string& other_local_tag)
	: other_id(other_local_tag),
	  sip_relay_only(true)
    {}

    virtual ~AmB2BSession();
};

class AmB2BCalleeSession;

class AmB2BCallerSession: public AmB2BSession
{
public:
    enum CalleeStatus {
	None=0,
	NoReply,
	Ringing,
	Connected
    };

private:
    // Callee Status
    CalleeStatus callee_status;
    AmSipRequest invite_req;

    void relayEvent(AmEvent* ev);
    void createCalleeSession();
    int  reinviteCaller(const AmSipReply& callee_reply);

public:
    AmB2BCallerSession();
    
    CalleeStatus getCalleeStatus() { return callee_status; }

    virtual AmB2BCalleeSession* newCalleeSession();

    void connectCallee(const string& remote_party,
		       const string& remote_uri);

    const AmSipRequest& getOriginalRequest() { return invite_req; }

    // @see AmDialogState
    void onSessionStart(const AmSipRequest& req);

    // @see AmB2BSession
    void terminateLeg();
    void terminateOtherLeg();
    void onB2BEvent(B2BEvent* ev);

    void set_sip_relay_only(bool r) { sip_relay_only = r; }
};

class AmB2BCalleeSession: public AmB2BSession
{
public:
    AmB2BCalleeSession(const string& other_local_tag)
	: AmB2BSession(other_local_tag)
    {}

    AmB2BCalleeSession(const AmB2BCallerSession* caller)
	: AmB2BSession(caller->getLocalTag())
    {}

    void onB2BEvent(B2BEvent* ev);
};

#endif
