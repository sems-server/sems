/*
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. This program is released under
 * the GPL with the additional exemption that compiling, linking,
 * and/or using OpenSSL is allowed.
 *
 * For a license to use the SEMS software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * SEMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _AmSession_h_
#define _AmSession_h_

#include "AmRtpStream.h"
#include "AmThread.h"
#include "AmEventQueue.h"
#include "AmRtpAudio.h"
#include "AmDtmfDetector.h"
#include "AmSipMsg.h"
#include "AmSipHeaders.h"
#include "AmSipDialog.h"
#include "AmSipEvent.h"
#include "AmApi.h"
#include "AmSessionEventHandler.h"

#ifdef WITH_ZRTP
#include "zrtp/zrtp.h"
#endif

#include <string>
#include <vector>
#include <queue>
#include <map>
using std::string;
using std::vector;

class AmSessionFactory;
class AmDtmfEvent;

/** @file AmSession.h */

/* definition imported from Ser parser/msg_parser.h */
#define FL_FORCE_ACTIVE 2

#define AM_AUDIO_IN  0
#define AM_AUDIO_OUT 1


/**
 * \brief Implements the default behavior of one session
 * 
 * The session is identified by Call-ID, From-Tag and To-Tag.
 */
class AmSession : 
#ifndef SESSION_THREADPOOL
  public AmThread,
#endif
  public AmEventQueue, 
  public AmEventHandler,
  public AmSipDialogEventHandler
{
  AmMutex      audio_mut;
  // remote (to/from RTP) audio inout
  AmAudio*     input;
  AmAudio*     output;

  // local (to/from audio dev) audio inout
  AmAudio*     local_input;
  AmAudio*     local_output;

  bool use_local_audio[2];
protected:
  vector<SdpPayload *>  m_payloads;
  bool         negotiate_onreply;

  friend class AmRtpAudio;

  /** get new RTP format for the session */
  virtual AmAudioRtpFormat* getNewRtpFormat();

private:
  AmDtmfDetector   m_dtmfDetector;
  AmDtmfEventQueue m_dtmfEventQueue;
  bool m_dtmfDetectionEnabled;

  enum ProcessingStatus { 
    SESSION_PROCESSING_EVENTS = 0,
    SESSION_WAITING_DISCONNECTED,
    SESSION_ENDED_DISCONNECTED
  };
  ProcessingStatus processing_status;

#ifndef SESSION_THREADPOOL
  /** @see AmThread::run() */
  void run();
  void on_stop();
#else
public:
  void start();
  bool is_stopped();

private:
  void stop();
  void* _pid;
#endif

  /** @return whether startup was successful */
  bool startup();

  /** @return whether session continues running */
  bool processingCycle();

  /** clean up session */
  void finalize();

  /** process pending events,  
      @return whether everything went smoothly */
  bool processEventsCatchExceptions();


  AmCondition<bool> sess_stopped;
  AmCondition<bool> detached;

  static volatile unsigned int session_num;
  static AmMutex session_num_mut;

  friend class AmMediaProcessor;
  friend class AmMediaProcessorThread;
  friend class AmSessionContainer;
  friend class AmSessionFactory;
  friend class AmSessionProcessorThread;

  auto_ptr<AmRtpAudio> _rtp_str;

  AmDynInvoke* user_timer_ref;
  
  void getUserTimerInstance();
protected:
  AmSdp               sdp;

  /** this is the group the media is processed with 
      - by default local tag */
  string callgroup;

  /** do accept early session? */
  bool accept_early_session;

  /** Local IP interface to be used for RTP streams */
  int rtp_interface;

  vector<AmSessionEventHandler*> ev_handlers;

public:

  enum SessionRefreshMethod {
    REFRESH_REINVITE = 0,      // use reinvite
    REFRESH_UPDATE,            // use update
    REFRESH_UPDATE_FB_REINV    // use update or fallback to reinvite
  };
  /** currently selected session refresh method */
  SessionRefreshMethod refresh_method;

  /** update selected session refresh method from remote capabilities */
  void updateRefreshMethod(const string& headers);

  AmRtpAudio* RTPStream();

#ifdef WITH_ZRTP
  zrtp_conn_ctx_t*    zrtp_session; // ZRTP session
  zrtp_stream_ctx_t*  zrtp_audio;   // ZRTP stream for audio

  /** must be set before session is started! i.e. in constructor */
  bool enable_zrtp;
#endif

  AmSipDialog         dlg;

  /** 
   * \brief Exception occured in a Session
   * 
   * Session (creation) should be aborted and replied with code/reason.
   */
  struct Exception {
    int code;
    string reason;
    string hdrs;
    Exception(int c, string r, string h="") : code(c), reason(r), hdrs(h) {}
  };

  /** 
   * Session constructor.
   */
  AmSession();

  virtual ~AmSession();

  /**
   * @see AmEventHandler
   */
  virtual void process(AmEvent*);

  /**
   * add a handler which will be called 
   * for all events in session
   * 
   * @see AmSessionEventHandler
   */
  void addHandler(AmSessionEventHandler*);

  /**
   * Set the call group for this call; calls in the same
   * group are processed by the same media processor thread.
   * 
   * Note: this must be set before inserting 
   * the session to the MediaProcessor!
   */
  void setCallgroup(const string& cg);

  /** get the callgroup @return callgroup */
  string getCallgroup();

  /** This function removes the session from 
   *  the media processor and adds it again. 
   */
  void changeCallgroup(const string& cg);

  /**
   * Accept the SDP proposal
   * thus setting up audio stream
   */
  int acceptAudio(const string& body,
		  const string& hdrs = "",
		  string*       sdp_reply=0);

  /**
   * Lock audio input & output
   * (inclusive RTP stream)
   */
  void lockAudio();

  /**
   * Unlock audio input & output
   * (inclusive RTP stream)
   */
  void unlockAudio();

  /**
   * Audio input getter .
   * Note: audio must be locked!
   */
  AmAudio* getInput() { return input; }
  /**
   * Audio output getter.
   * Note: audio must be locked!
   */
  AmAudio* getOutput(){ return output;}

  /**
   * Audio input & output set methods.
   * Note: audio will be locked by the methods.
   */
  void setInput(AmAudio* in);
  void setOutput(AmAudio* out);
  void setInOut(AmAudio* in, AmAudio* out);


  /**
   * Local audio input getter .
   * Note: audio must be locked!
   */
  AmAudio* getLocalInput() { return local_input; }
  /**
   * Local audio output getter.
   * Note: audio must be locked!
   */
  AmAudio* getLocalOutput() { return local_output;}

  /**
   * Local audio input & output set methods.
   * Note: audio will be locked by the methods.
   */
  void setLocalInput(AmAudio* in);
  void setLocalOutput(AmAudio* out);
  void setLocalInOut(AmAudio* in, AmAudio* out);

  /** this switches between local and remote 
   * audio inout 
   */
  void setAudioLocal(unsigned int dir, bool local);
  bool getAudioLocal(unsigned int dir);

  /**
   * Clears input & ouput (no need to lock)
   */
  void clearAudio();

  /** setter for rtp_str->mute */
  void setMute(bool mute) { RTPStream()->mute = mute; }

  /** setter for rtp_str->receiving */
  void setReceiving(bool receive) { RTPStream()->receiving = receive; }

  /** Gets the Session's call ID */
  const string& getCallID() const;

  /** Gets the Session's remote tag */
  const string& getRemoteTag()const ;

  /** Gets the Session's local tag */
  const string& getLocalTag() const;

  /** Sets the Session's local tag if not set already */
  void setLocalTag();

  /** Sets the Session's local tag */
  void setLocalTag(const string& tag);

  /** Sets the URI for the session */
  void setUri(const string& uri);

  /** Gets the current RTP payload */
  const vector<SdpPayload*>& getPayloads();

  /** Gets the port number of the remote part of the session */
  int getRPort();

  /** Set whether on positive reply session should be negotiated */
  void setNegotiateOnReply(bool n) { negotiate_onreply = n; }

  /** get the payload provider for the session */
  virtual AmPayloadProviderInterface* getPayloadProvider();

  /** handle SDP negotiation: only for INVITEs & re-INVITEs */
  virtual void negotiate(const string& sdp_body,
			 bool force_symmetric_rtp,
			 string* sdp_reply);

  /** refresh the session - re-INVITE or UPDATE*/
  virtual bool refresh(int flags = 0);

  /** send an UPDATE in the session */
  virtual int sendUpdate(const string &cont_type, const string &body, const string &hdrs);

  /** send a Re-INVITE (if connected) */
  virtual int sendReinvite(bool updateSDP = true, const string& headers = "",
			   int flags = 0);

  /** send an INVITE */
  virtual int sendInvite(const string& headers = "");

  /** set the session on/off hold */
  virtual void setOnHold(bool hold);

  /**
   * Destroy the session.
   * It causes the session to be erased from the active session list
   * and added to the dead session list.
   * @see AmSessionContainer
   */
  virtual void destroy();

  /**
   * Signals the session it should stop.
   * This will cause the session to be able 
   * to exit the main loop.
   * If wakeup is set, a bogus event will 
   * be sent to wake up the session.
   */
  void setStopped(bool wakeup = false);

  /**
   * Has the session already been stopped ?
   */
  bool getStopped() { return sess_stopped.get(); }

  /** Is the session detached from media processor? */
  bool getDetached() { return detached.get(); }

  /**
   * Creates a new Id which can be used within sessions.
   */
  static string getNewId();

  /**
   * Gets the number of running sessions
   */
  static unsigned int getSessionNum();

  /**
   * Entry point for DTMF events
   */
  void postDtmfEvent(AmDtmfEvent *);

  void processDtmfEvents();

  void setInbandDetector(Dtmf::InbandDetectorType t);
  bool isDtmfDetectionEnabled() { return m_dtmfDetectionEnabled; }
  void setDtmfDetectionEnabled(bool e) { m_dtmfDetectionEnabled = e; }
  void putDtmfAudio(const unsigned char *buf, int size, int user_ts);

  /**
   * send a DTMF as RTP payload (RFC4733)
   * @param event event ID (e.g. key press), see rfc
   * @param duration_ms duration in milliseconds
   */
  void sendDtmf(int event, unsigned int duration_ms);

  /* ---- general purpose application level timers ------------ */

  /** check for support of timers
    @return true if application level timers are supported
   */
  static bool timersSupported();

  /**
     set a Timer
     @param timer_id the ID of the timer (<0 for system timers)
     @param timeout timeout in seconds
     @return true on success
  */
  virtual bool setTimer(int timer_id, unsigned int timeout);

  /**
     remove a Timer
     @param timer_id the ID of the timer (<0 for system timers)
     @return true on success
  */
  virtual bool removeTimer(int timer_id);

  /**
     remove all Timers
     @return true on success
     Note: this doesn't clear timer events already in the 
           event queue
  */
  virtual bool removeTimers();

  /* ---------- event handlers ------------------------- */

  /** DTMF event handler for apps to use*/
  virtual void onDtmf(int event, int duration);

  /**
   * onStart will be called before everything else.
   */
  virtual void onStart() {}

  /**
   * onInvite will be called if an INVITE or re-INVITE
   * has been received for the session.
   */
  virtual void onInvite(const AmSipRequest& req);

  /**
   * onOutgoingInvite will be called if an INVITE 
   * is sent in the session.
   */
  virtual void onOutgoingInvite(const string& headers) { }

  /**
   * onCancel will be called if a CANCEL for a running
   * dialog has been received. At this point, the CANCEL
   * transaction has been replied with 200.
   *
   * A normal plug-in does not have to do anything special, 
   * as normal dialogs are immediatly replied with 200 
   * or error code. 
   *
   * Note: You are still responsible for responding the 
   *       initial transaction.
   */
  virtual void onCancel() {}

  /**
   * onSessionStart will be called after call setup.
   *
   * Throw AmSession::Exception if you want to 
   * signal any error.
   * 
   * Warning:
   *   Sems will NOT send any BYE on his own.
   */
  virtual void onSessionStart(const AmSipRequest& req) {}

  /**
   * onSessionStart method for calls originating 
   * from SEMS.
   *
   * Throw AmSession::Exception if you want to 
   * signal any error.
   * 
   * Warning:
   *   Sems will NOT send any BYE on his own.
   */
  virtual void onSessionStart(const AmSipReply& reply) {}


  /**
   * onEarlySessionStart will be called after 
   * 183 early media reply is received and early session 
   * is setup, if accept_early_session is set.
   */
  virtual void onEarlySessionStart(const AmSipReply& reply) {}

  /**
   * onRinging will be called after 180 is received. 
   * If local audio is set up, session is added to media processor.
   */
  virtual void onRinging(const AmSipReply& reply) {}

  /**
   * onBye is called whenever a BYE request is received. 
   */
  virtual void onBye(const AmSipRequest& req);

  /** Entry point for SIP Requests   */
  virtual void onSipRequest(const AmSipRequest& req);

  /** Entry point for SIP Replies   */
  virtual void onSipReply(const AmSipReply& reply, int old_dlg_status,
			      const string& trans_method);

  /** 2xx reply has been received for an INVITE transaction */
  virtual void onInvite2xx(const AmSipReply& reply);


  virtual void onInvite1xxRel(const AmSipReply &);

  /** Hook called when an answer for a locally sent PRACK is received */
  virtual void onPrack2xx(const AmSipReply &);

  virtual void onFailure(AmSipDialogEventHandler::FailureCause cause, 
      const AmSipRequest*, const AmSipReply*);
  
#if 0
  /** missing 2xx-ACK */
  virtual void onNo2xxACK(unsigned int cseq);
  /** missing non-2xx-ACK */
  virtual void onNoErrorACK(unsigned int cseq);
#else
  virtual void onNoAck(unsigned int cseq);
  virtual void onNoPrack(const AmSipRequest &req, const AmSipReply &rpl);
#endif

  /**
   * Entry point for Audio events
   */
  virtual void onAudioEvent(AmAudioEvent* audio_ev);

  /**
   * entry point for system events
   */
  virtual void onSystemEvent(AmSystemEvent* ev);
  
#ifdef WITH_ZRTP
  /**
   * ZRTP events @see ZRTP
   */
  virtual void onZRTPEvent(zrtp_event_t event, zrtp_stream_ctx_t *stream_ctx);
#endif

  /** This callback is called if RTP timeout encountered */
  virtual void onRtpTimeout();

  /** This callback is called if session
      timeout encountered (session timers) */
  virtual void onSessionTimeout();

  /* Called by AmSipDialog when a request is sent */
  virtual void onSendRequest(const string& method,
			     const string& content_type,
			     const string& body,
			     string& hdrs,
			     int flags,
			     unsigned int cseq);

  /* Called by AmSipDialog when a reply is sent */
  virtual void onSendReply(const AmSipRequest& req,
			   unsigned int  code,
			   const string& reason,
			   const string& content_type,
			   const string& body,
			   string& hdrs,
			   int flags);

  /** 
   * called in the session thread before the session is destroyed,
   * i.e. after the main event loop has finished
   */
  virtual void onBeforeDestroy() { }

  // The IP address to put as c= in SDP bodies
  string advertisedIP();

  // The IP address to bind the RTP stream to
  string localRTPIP();

  /** format session id for debugging */
  string sid4dbg();
};

inline AmRtpAudio* AmSession::RTPStream() {
  if (NULL == _rtp_str.get()) {
    DBG("creating RTP stream instance for session [%p]\n", 
	this);
    if(rtp_interface < 0)
      rtp_interface = dlg.getOutboundIf();
    _rtp_str.reset(new AmRtpAudio(this,rtp_interface));
  }
  return _rtp_str.get();
}


#endif

/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 2
 * End:
 */

