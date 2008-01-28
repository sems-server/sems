/*
 * $Id$
 *
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of sems, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * sems is distributed in the hope that it will be useful,
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
#include "AmSipDialog.h"
#include "AmSipEvent.h"
#include "AmApi.h"

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
 * \brief Interface for SIP events signaling plugins implement
 *
 *  Signaling plugins must inherite from this class.
 */
class AmSessionEventHandler
  : public ArgObject
{
public:
  bool destroy;

  AmSessionEventHandler()
    : destroy(true) {}

  virtual ~AmSessionEventHandler() {}
  /* 
   * All the methods return true if event processing 
   * should stopped after calling them.
   */
  virtual bool process(AmEvent*);
  virtual bool onSipEvent(AmSipEvent*);
  virtual bool onSipRequest(const AmSipRequest&);
  virtual bool onSipReply(const AmSipReply&);

  virtual bool onSendRequest(const string& method, 
			     const string& content_type,
			     const string& body,
			     string& hdrs,
			     unsigned int cseq);

  virtual bool onSendReply(const AmSipRequest& req,
			   unsigned int  code,
			   const string& reason,
			   const string& content_type,
			   const string& body,
			   string& hdrs);
};



#define AM_AUDIO_IN  0
#define AM_AUDIO_OUT 1

/**
 * \brief Implements the default behavior of one session
 * 
 * The session is identified by Call-ID, From-Tag and To-Tag.
 */
class AmSession : public AmThread,
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

  vector<SdpPayload *>  m_payloads;
  bool         negotiate_onreply;

  AmDtmfDetector   m_dtmfDetector;
  AmDtmfEventQueue m_dtmfEventQueue;
  bool m_dtmfDetectionEnabled;

  vector<AmSessionEventHandler*> ev_handlers;

  /** @see AmThread::run() */
  void run();

  /** @see AmThread::on_stop() */
  void on_stop();

  AmCondition<bool> sess_stopped;
  AmCondition<bool> detached;

  friend class AmMediaProcessor;
  friend class AmMediaProcessorThread;
  friend class AmSessionContainer;
  friend class AmSessionFactory;
	
protected:
  AmSdp               sdp;
  AmRtpAudio          rtp_str;

  /** this is the group the media is processed with 
      - by default local tag */
  string callgroup;

  /** do accept early session? */
  bool accept_early_session;

public:
  AmSipDialog         dlg;

  /** 
   * \brief Exception occured in a Session
   * 
   * Session (creation) should be aborted and replied with code/reason.
   */
  struct Exception {
    int code;
    string reason;
    Exception(int c, string r) : code(c), reason(r) {}
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
   * Set the call group for this call. 
   * 
   * Note: this must be set before inserting 
   * the session to the scheduler!
   */
  void setCallgroup(const string& cg);
  string getCallgroup() { return callgroup; }

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
  AmAudio* getLocalOutput(){ return local_output;}

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
  void setMute(bool mute) { rtp_str.mute = mute; }

  /** setter for rtp_str->receiving */
  void setReceiving(bool receive) { rtp_str.receiving = receive; }

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

  /** handle SDP negotiation: only for INVITEs & re-INVITEs */
  virtual void negotiate(const string& sdp_body,
			 bool force_symmetric_rtp,
			 string* sdp_reply);

  /** send an UPDATE in the session */
  void sendUpdate();
  /** send a Re-INVITE (if connected) */
  void sendReinvite(bool updateSDP = true, const string& headers = "");
  /** send an INVITE */
  int sendInvite(const string& headers = "");

  /** set the session on/off hold */
  void setOnHold(bool hold);

  /**
   * Destroy the session.
   * It causes the session to be erased from the active session list
   * and added to the dead session list.
   * @see AmSessionContainer
   */
  void destroy();

  /**
   * Signals the session it should stop.
   * This will cause the session to be able 
   * to exit the main loop.
   */
  void setStopped() { sess_stopped.set(true); }

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
   * Entry point for DTMF events
   */
  void postDtmfEvent(AmDtmfEvent *);

  void processDtmfEvents();

  void setInbandDetector(Dtmf::InbandDetectorType t);
  bool isDtmfDetectionEnabled() { return m_dtmfDetectionEnabled; }
  void setDtmfDetectionEnabled(bool e) { m_dtmfDetectionEnabled = e; }
  void putDtmfAudio(const unsigned char *buf, int size, int user_ts);
  /** event handler for apps to use*/
  virtual void onDtmf(int event, int duration);

  /**
   * onStart will be called before everything else.
   */
  virtual void onStart(){}

  /**
   * onInvite will be called if an INVITE or re-INVITE
   * has been received for the session.
   */
  virtual void onInvite(const AmSipRequest& req);

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
  virtual void onCancel(){}

  /**
   * onSessionStart will be called after call setup.
   *
   * Throw AmSession::Exception if you want to 
   * signal any error.
   * 
   * Warning:
   *   Sems will NOT send any BYE on his own.
   */
  virtual void onSessionStart(const AmSipRequest& req){}

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
  virtual void onSessionStart(const AmSipReply& reply){}


  /**
   * onEarlySessionStart will be called after 
   * 183 early media reply is received and early session 
   * is setup, if accept_early_session is set.
   */
  virtual void onEarlySessionStart(const AmSipReply& reply){}

  /**
   * onRinging will be called after 180 is received. 
   * If local audio is set up, session is added to scheduler. 
   */
  virtual void onRinging(const AmSipReply& reply){}

  /**
   * @see AmDialogState
   */
  virtual void onBye(const AmSipRequest& req);

  /**
   * Entry point for SIP events
   */
  virtual void onSipEvent(AmSipEvent* sip_ev);
  virtual void onSipRequest(const AmSipRequest& req);
  virtual void onSipReply(const AmSipReply& reply);


  /** This callback is called if RTP timeout encountered */
  virtual void onRtpTimeout();

  /* only called by AmSipDialog */
  virtual void onSendRequest(const string& method,
			     const string& content_type,
			     const string& body,
			     string& hdrs,
			     unsigned int cseq);

  virtual void onSendReply(const AmSipRequest& req,
			   unsigned int  code,
			   const string& reason,
			   const string& content_type,
			   const string& body,
			   string& hdrs);
};

#endif

// Local Variables:
// mode:C++
// End:

