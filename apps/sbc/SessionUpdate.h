#ifndef __SESSION_UPDATE_H
#define __SESSION_UPDATE_H

#include <string>
#include "AmMimeBody.h"
#include "AmAppTimer.h"

class CallLeg;

/* interface to be implemented by re-applicable operations on a session
 * expected usage: the operation generates a reINVITE and if this reINVITE fails
 * with specific error code (491, ...) the operation can be re-applied
 *
 * Note that only one session update operation may be running, others need
 * to wait for finishing the current one.
 * */
class SessionUpdate
{
  private:
    int request_cseq;

  protected:
    void setCSeq(int cseq) { request_cseq = cseq; }

    SessionUpdate(): request_cseq(-1) { }

  public:
    // used to (re)apply session update
    // returns the CSeq of generated request (reINVITE/...)
    virtual void apply(CallLeg *call) = 0;

//    virtual void onSipReply(CallLeg *call, AmSipReply &reply);
    virtual ~SessionUpdate() { }

    // check whether update request was sent out
    bool hasCSeq() const { return request_cseq >= 0; }

    // check the request cseq value
    bool hasCSeq(int cseq) const { return request_cseq == cseq; }

    // reset internal state to be prepared for retrying the update operation
    virtual void reset() { setCSeq(-1); }

};

class PutOnHold: public SessionUpdate
{
  public:
    virtual void apply(CallLeg *call);
};

class ResumeHeld: public SessionUpdate
{
  public:
    virtual void apply(CallLeg *call);
};

class Reinvite: public SessionUpdate
{
    std::string hdrs;
    AmMimeBody body;
    unsigned r_cseq;
    bool relayed_invite;
    bool establishing;

  public:
    virtual void apply(CallLeg *call);

    Reinvite(const std::string& _hdrs, const AmMimeBody& _body,
        bool _establishing = false,
        bool _relayed_invite = false, unsigned int _r_cseq = 0):
          hdrs(_hdrs), body(_body), r_cseq(_r_cseq),
          relayed_invite(_relayed_invite), establishing(_establishing) { }
};

class SessionUpdateTimer: public DirectAppTimer
{
  private:
    std::string ltag;
    bool has_started;

  public:
    SessionUpdateTimer(): has_started(false) { }
    ~SessionUpdateTimer() { if (has_started) AmAppTimer::instance()->removeTimer(this); }

    void fire();
    bool started() { return has_started; }

    // start the timer (local tag is supplied here because it may change during
    // call legs's lifetime)
    void start(const std::string &_ltag, double delay);
};

#endif
