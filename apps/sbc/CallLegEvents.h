#ifndef __CALL_LEG_EVENTS_H
#define __CALL_LEG_EVENTS_H

// TODO: global event numbering
enum {
  ConnectLeg = B2BMsgBody + 16,
  ReconnectLeg,
  ReplaceLeg,
  ReplaceInProgress,
  DisconnectLeg,
  ChangeRtpModeEventId,
  ResumeHeldLeg,
  ApplyPendingUpdatesEventId
};

#define LAST_B2B_CALL_LEG_EVENT_ID ApplyPendingUpdatesEventId

struct ConnectLegEvent: public B2BEvent
{
  AmMimeBody body;
  string hdrs;

  unsigned int r_cseq;
  unsigned int r_max_forwards;
  bool relayed_invite;

  // constructor from relayed INVITE request
  ConnectLegEvent(const AmSipRequest &_relayed_invite):
    B2BEvent(ConnectLeg),
    body(_relayed_invite.body),
    hdrs(_relayed_invite.hdrs),
    r_cseq(_relayed_invite.cseq),
    r_max_forwards(_relayed_invite.max_forwards),
    relayed_invite(true)
  { }

  // constructor from generated INVITE (for example blind call transfer)
ConnectLegEvent(const string &_hdrs, const AmMimeBody &_body, unsigned int _max_forwards):
    B2BEvent(ConnectLeg),
    body(_body),
    hdrs(_hdrs),
    r_cseq(0),
    r_max_forwards(_max_forwards),
    relayed_invite(false)
  { }
};

/** B2B event which sends another event back if it was or was not processed.
 * (note that the back events need to be created in advance because we can not
 * use overriden virtual methods in destructor (which is the only place which
 * will be called for sure) */
struct ReliableB2BEvent: public B2BEvent
{
  private:
    bool processed;

    B2BEvent *unprocessed_reply; //< reply to be sent back if the original event was not processed
    B2BEvent *processed_reply; //< event sent back if the original event was processed
    string sender; // sender will be filled when sending the event out

  public:

    ReliableB2BEvent(int ev_id, B2BEvent *_processed, B2BEvent *_unprocessed):
      B2BEvent(ev_id), processed(false), unprocessed_reply(_unprocessed), processed_reply(_processed) { }
    ReliableB2BEvent(int ev_id, B2BEventType ev_type, B2BEvent *_processed, B2BEvent *_unprocessed):
      B2BEvent(ev_id, ev_type), processed(false), unprocessed_reply(_unprocessed), processed_reply(_processed) { }
    void markAsProcessed() { processed = true; }
    void setSender(const string &tag) { sender = tag; }
    virtual ~ReliableB2BEvent();
};

struct ReconnectLegEvent: public ReliableB2BEvent
{
  AmMimeBody body;
  string hdrs;

  unsigned int r_cseq;
  bool relayed_invite;

  AmB2BMedia *media; // avoid direct access to this
  AmB2BSession::RTPRelayMode rtp_mode;
  string session_tag;
  enum Role { A, B } role; // reconnect as A or B leg

  void setMedia(AmB2BMedia *m, AmB2BSession::RTPRelayMode _mode) { media = m; if (media) media->addReference(); rtp_mode = _mode; }

  ReconnectLegEvent(const string &tag, const AmSipRequest &relayed_invite):
    ReliableB2BEvent(ReconnectLeg, NULL, new B2BEvent(B2BTerminateLeg) /* TODO: choose a better one */),
    body(relayed_invite.body),
    hdrs(relayed_invite.hdrs),
    r_cseq(relayed_invite.cseq),
    relayed_invite(true),
    media(NULL),
    rtp_mode(AmB2BSession::RTP_Direct),
    session_tag(tag),
    role(B) // we have relayed_invite (only in A leg) thus reconnect as regular B leg
  { setSender(tag); }

  ReconnectLegEvent(Role _role, const string &tag, const string &_hdrs, const AmMimeBody &_body):
    ReliableB2BEvent(ReconnectLeg, NULL, new B2BEvent(B2BTerminateLeg) /* TODO: choose a better one */),
    body(_body),
    hdrs(_hdrs),
    r_cseq(0),
    relayed_invite(false),
    media(NULL),
    rtp_mode(AmB2BSession::RTP_Direct),
    session_tag(tag),
    role(_role)
  { setSender(tag); }

    virtual ~ReconnectLegEvent() { if (media) media->releaseReference(); }
};


/** Call leg receiving ReplaceLegEvent should replace itself with call leg from
 * the event parameters. (it terminates itself and forwards ReconnectLegEvent to
 * the call leg identified by other_id) */
struct ReplaceLegEvent: public ReliableB2BEvent
{
  private:
    ReconnectLegEvent *ev;

  public:
    ReplaceLegEvent(const string &tag, const AmSipRequest &relayed_invite, AmB2BMedia *m, AmB2BSession::RTPRelayMode mode):
      ReliableB2BEvent(ReplaceLeg, NULL, new B2BEvent(B2BTerminateLeg))
    { ev = new ReconnectLegEvent(tag, relayed_invite); ev->setMedia(m, mode); setSender(tag); }

    ReplaceLegEvent(const string &tag, ReconnectLegEvent *e):
      ReliableB2BEvent(ReplaceLeg, NULL, new B2BEvent(B2BTerminateLeg)),
      ev(e)
    { setSender(tag); }

    ReconnectLegEvent *getReconnectEvent() { ReconnectLegEvent *e = ev; ev = NULL; return e; }
    virtual ~ReplaceLegEvent() { if (ev) delete ev; }
};

struct ReplaceInProgressEvent: public B2BEvent
{
  string dst_session; // session to be connected to

  ReplaceInProgressEvent(const string &_dst_session):
      B2BEvent(ReplaceInProgress), dst_session(_dst_session) { }
};

struct DisconnectLegEvent: public B2BEvent
{
  bool put_remote_on_hold;
  bool preserve_media_session;
  DisconnectLegEvent(bool _put_remote_on_hold, bool _preserve_media_session = false):
    B2BEvent(DisconnectLeg),
    put_remote_on_hold(_put_remote_on_hold),
    preserve_media_session(_preserve_media_session) { }
};

/* we don't need to have 'reliable event' for this because we are always
 * connected to CallLeg, right? */
struct ChangeRtpModeEvent: public B2BEvent
{
  AmB2BSession::RTPRelayMode new_mode;
  AmB2BMedia *media; // avoid direct access to this

  ChangeRtpModeEvent(AmB2BSession::RTPRelayMode _new_mode, AmB2BMedia *_media):
    B2BEvent(ChangeRtpModeEventId), new_mode(_new_mode), media(_media)
    { if (media) media->addReference(); }

    virtual ~ChangeRtpModeEvent() { if (media) media->releaseReference(); }
};

struct ResumeHeldEvent: public B2BEvent
{
  ResumeHeldEvent(): B2BEvent(ResumeHeldLeg) { }
};

struct ApplyPendingUpdatesEvent: public B2BEvent
{
  ApplyPendingUpdatesEvent(): B2BEvent(ApplyPendingUpdatesEventId) { }
};

#endif
