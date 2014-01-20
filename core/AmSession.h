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
#include "AmMediaProcessor.h"

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


/**
 * \brief Implements the default behavior of one session
 * 
 * The session is identified by Call-ID, From-Tag and To-Tag.
 */
class AmSession : 
  public AmObject,
#ifndef SESSION_THREADPOOL
  public AmThread,
#endif
  public AmEventQueue, 
  public AmEventHandler,
  public AmSipDialogEventHandler,
  public AmMediaSession,
  public AmDtmfSink
{
  AmMutex      audio_mut;

protected:
  vector<SdpPayload *>  m_payloads;
  //bool         negotiate_onreply;

  friend class AmRtpAudio;

  /** get new RTP format for the session */
  //virtual AmAudioRtpFormat* getNewRtpFormat();

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

  static void session_started();
  static void session_stopped();

  static volatile unsigned int session_num;
  static volatile unsigned int max_session_num;
  static volatile unsigned long long avg_session_num;
  static AmMutex session_num_mut;

  friend class AmMediaProcessor;
  friend class AmMediaProcessorThread;
  friend class AmSessionContainer;
  friend class AmSessionFactory;
  friend class AmSessionProcessorThread;

  auto_ptr<AmRtpAudio> _rtp_str;

  /** Application parameters passed through P-App-Param HF */
  map<string,string> app_params;

  /** Sets the application parameters from the original request */
  void setAppParams(const AmSipRequest& req);

protected:

  AmCondition<bool> sess_stopped;

  /** this is the group the media is processed with 
      - by default local tag */
  string callgroup;

  /** do accept early session? */
  bool accept_early_session;

  /** Local IP interface to be used for RTP streams */
  int rtp_interface;

  /** Session event handlers (ex: session timer, UAC auth, etc...) */
  vector<AmSessionEventHandler*> ev_handlers;

  AmAudio *input, *output;

  virtual AmSipDialog* createSipDialog();

  /** process pending events,  
      @return whether everything went smoothly */
  virtual bool processEventsCatchExceptions();

  /** @return whether startup was successful */
  bool startup();

  /** @return whether session continues running */
  virtual bool processingCycle();

  /** clean up session */
  void finalize();

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
  bool hasRtpStream() { return _rtp_str.get() != NULL; }

#ifdef WITH_ZRTP
  zrtp_conn_ctx_t*    zrtp_session; // ZRTP session
  zrtp_stream_ctx_t*  zrtp_audio;   // ZRTP stream for audio

  /** must be set before session is started! i.e. in constructor */
  bool enable_zrtp;
#endif

  AmSipDialog* dlg;

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
  AmSession(AmSipDialog* dlg=NULL);

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

  /* ----         media processing                    ---- */

  /** start processing media - add to media processor */
  void startMediaProcessing();

  /** stop processing media - remove from media processor */
  void stopMediaProcessing();

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

  /**
   * change the callgroup
   *
   * This function removes the session from
   * the media processor and adds it again.
   */
  void changeCallgroup(const string& cg);

  /* ----         audio input and output        ---- */

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
  AmAudio* getOutput() { return output; }

  /**
   * Audio input & output set methods.
   * Note: audio will be locked by the methods.
   */
  void setInput(AmAudio* in);
  void setOutput(AmAudio* out);
  void setInOut(AmAudio* in, AmAudio* out);

  /** checks if input/output is set, might be overidden! */
  virtual bool isAudioSet();

  /**
   * Clears input & ouput (no need to lock)
   */
  virtual void clearAudio();

  /** setter for rtp_str->mute */
  void setMute(bool mute) { RTPStream()->mute = mute; }

  /** setter for rtp_str->receiving */
  void setReceiving(bool receive) { RTPStream()->setReceiving(receive); }

  /** setter for rtp_str->force_receive_dtmf*/
  void setForceDtmfReceiving(bool receive) { RTPStream()->force_receive_dtmf = receive; }

  /* ----         SIP dialog attributes                  ---- */

  /** Gets the Session's call ID */
  const string& getCallID() const;

  /** Gets the Session's remote tag */
  const string& getRemoteTag()const ;

  /** Gets the Session's local tag */
  const string& getLocalTag() const;

  /** Gets the branch param of the first via in the original INVITE*/
  const string& getFirstBranch() const;

  /** Sets the Session's local tag if not set already */
  void setLocalTag();

  /** Sets the Session's local tag */
  void setLocalTag(const string& tag);

  /** Sets the URI for the session */
  void setUri(const string& uri);

  /* ----         RTP stream attributes                  ---- */

  /** Gets the current RTP payload */
  const vector<SdpPayload*>& getPayloads();

  /** Gets the port number of the remote part of the session */
  int getRPort();

  /* ----         Call control                         ---- */

  /** refresh the session - re-INVITE or UPDATE*/
  virtual bool refresh(int flags = 0);

  /** send an UPDATE in the session */
  virtual int sendUpdate(const AmMimeBody* body, const string &hdrs);

  /** send a Re-INVITE (if connected) */
  virtual int sendReinvite(bool updateSDP = true, const string& headers = "",
			   int flags = 0);

  /** send an INVITE */
  virtual int sendInvite(const string& headers = "");

  /** set the session on/off hold */
  virtual void setOnHold(bool hold);

  /** update UAC trans state reference from old_cseq to new_cseq
      e.g. if uac_auth or session_timer have resent a UAC request
   */
  virtual void updateUACTransCSeq(unsigned int old_cseq, unsigned int new_cseq) { }

  /* ----         Householding                              ---- */

  /**
   * Get a session parameter ('P-App-Param' HF, etc...)
   */
  string getAppParam(const string& param_name) const;

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
  virtual void setStopped(bool wakeup = false);

  /**
   * Has the session already been stopped ?
   */
  bool getStopped() { return sess_stopped.get(); }

  /* ----         Statistics                    ---- */
  /**
   * Gets the number of running sessions
   */
  static unsigned int getSessionNum();
  /**
   * Gets the maximum of running sessions since last query
   */
  static unsigned int getMaxSessionNum();
  /**
   * Gets the average of running sessions since last query
   */
  static unsigned int getAvgSessionNum();

  /* ----         DTMF                          ---- */
  /**
   * Entry point for DTMF events
   */
  void postDtmfEvent(AmDtmfEvent *);

  void setInbandDetector(Dtmf::InbandDetectorType t);
  bool isDtmfDetectionEnabled() { return m_dtmfDetectionEnabled; }
  void setDtmfDetectionEnabled(bool e) { m_dtmfDetectionEnabled = e; }
  void putDtmfAudio(const unsigned char *buf, int size, unsigned long long system_ts);

  /**
   * send a DTMF as RTP payload (RFC4733)
   * @param event event ID (e.g. key press), see rfc
   * @param duration_ms duration in milliseconds
   */
  void sendDtmf(int event, unsigned int duration_ms);

  /* ---- general purpose application level timers ------------ */

  /** Deprecated: check for support of timers
    @return always true
   */
  static bool timersSupported();

  /**
     set a Timer
     @param timer_id the ID of the timer (<0 for system timers)
     @param timeout timeout in seconds (fractal value allowed)
     @return true on success
  */
  virtual bool setTimer(int timer_id, double timeout);

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
   * onStop will be called once session is marked to be stopped (called only
   * once).
   */
  virtual void onStop() {}

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
  virtual void onCancel(const AmSipRequest& req);

  /**
   * onRinging will be called after 180 is received. 
   * If local audio is set up, session is added to media processor.
   */
  virtual void onRinging(const AmSipReply& reply) {}

  /**
   * onBye is called whenever a BYE request is received. 
   */
  virtual void onBye(const AmSipRequest& req);

  /** remote side is unreachable - 408/481 reply received */
  virtual void onRemoteDisappeared(const AmSipReply&);

  /** Entry point for SIP Requests   */
  virtual void onSipRequest(const AmSipRequest& req);

  /** Entry point for SIP Replies   */
  virtual void onSipReply(const AmSipRequest& req, const AmSipReply& reply, 
			  AmBasicSipDialog::Status old_dlg_status);

  /** 2xx reply has been received for an INVITE transaction */
  virtual void onInvite2xx(const AmSipReply& reply);

  virtual void onInvite1xxRel(const AmSipReply &);

  /** answer for a locally sent PRACK is received */
  virtual void onPrack2xx(const AmSipReply &);

  virtual void onFailure();
  
  virtual void onNoAck(unsigned int cseq);
  virtual void onNoPrack(const AmSipRequest &req, const AmSipReply &rpl);

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
  virtual void onSendRequest(AmSipRequest& req, int& flags);

  /** Called by AmSipDialog when a reply is sent */
  virtual void onSendReply(const AmSipRequest& req, AmSipReply& reply, int& flags);

  /** Hook called when an SDP offer is required */
  virtual bool getSdpOffer(AmSdp& offer);

  /** Hook called when an SDP offer is required */
  virtual bool getSdpAnswer(const AmSdp& offer, AmSdp& answer);

  /** Hook called when an SDP OA transaction has been completed */
  virtual int onSdpCompleted(const AmSdp& offer, const AmSdp& answer);

  /** Hook called when an early session starts (SDP OA completed + dialog in early state) */
  virtual void onEarlySessionStart();

  /** Hook called when the session creation is completed (INV trans replied with 200) */
  virtual void onSessionStart();

  /** 
   * called in the session thread before the session is destroyed,
   * i.e. after the main event loop has finished
   */
  virtual void onBeforeDestroy() { }

  // The IP address to put as c= in SDP bodies
  string advertisedIP(int addrType = AT_NONE);

  // IP address used to bind the RTP socket
  string localMediaIP(int addrType = AT_NONE);

  /** format session id for debugging */
  string sid4dbg();

  /**
   * Creates a new Id which can be used within sessions.
   */
  static string getNewId();

  /* ----------------- media processing interface ------------------- */

public: 
  virtual int readStreams(unsigned long long ts, unsigned char *buffer);
  virtual int writeStreams(unsigned long long ts, unsigned char *buffer);
  virtual void clearRTPTimeout() { RTPStream()->clearRTPTimeout(); }
  virtual void processDtmfEvents();

  /**
   * Call-backs used by RTP stream(s)
   * 
   * Note: these methods will be called from the RTP receiver thread.
   */
  virtual bool onBeforeRTPRelay(AmRtpPacket* p, sockaddr_storage* remote_addr)
  { return true; }

  virtual void onAfterRTPRelay(AmRtpPacket* p, sockaddr_storage* remote_addr) {}

  int getRtpInterface();
};

inline AmRtpAudio* AmSession::RTPStream() {
  if (NULL == _rtp_str.get()) {
    DBG("creating RTP stream instance for session [%p]\n", 
	this);
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

