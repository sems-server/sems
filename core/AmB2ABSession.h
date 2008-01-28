/*
 * $Id: AmB2ABSession.h 145 2006-11-26 00:01:18Z sayer $
 *
 * Copyright (C) 2006-2007 iptego GmbH
 *
 * This file is part of sems, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For a license to use the sems software under conditions
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
/** @file AmB2ABSession.h */
#ifndef B2ABSession_H
#define B2ABSession_H

#include "AmSession.h"
#include "AmSipDialog.h"
#include "AmAdvancedAudio.h"
#include "SampleArray.h"
#include <string>
using std::string;

class AmAudioDelayBridge;
class AmSessionAudioConnector;

enum { B2ABTerminateLeg, 
       B2ABConnectLeg, 
       B2ABConnectAudio,
       B2ABConnectOtherLegFailed,
       B2ABConnectOtherLegException,
       B2ABOtherLegRinging
};

/** \brief base class for event in B2AB session */
struct B2ABEvent: public AmEvent
{
  B2ABEvent(int ev_id) 
    : AmEvent(ev_id)
  {}
};

/** \brief event fired if B leg is ringing in b2ab session */
struct B2ABOtherLegRingingEvent: public B2ABEvent
{
  B2ABOtherLegRingingEvent()
    : B2ABEvent(B2ABOtherLegRinging)
  {}
};

/** \brief trigger connecting the audio in B2AB session */
struct B2ABConnectAudioEvent: public B2ABEvent
{
  AmSessionAudioConnector* connector;
  bool first;

  B2ABConnectAudioEvent(AmSessionAudioConnector* connector)
    : B2ABEvent(B2ABConnectAudio),
       connector(connector)
  {}
};

/** \brief trigger connecting the callee leg in B2AB session */
struct B2ABConnectLegEvent: public B2ABEvent
{
  string remote_party;
  string remote_uri;
  string local_party;
  string local_uri;
  string callgroup;

  B2ABConnectLegEvent(const string& remote_party,
		      const string& remote_uri,
		      const string& local_party,
		      const string& local_uri,
		      const string& callgroup)
    : B2ABEvent(B2ABConnectLeg),
       remote_party(remote_party),
       remote_uri(remote_uri),
       local_party(local_party),
       local_uri(local_uri),
       callgroup(callgroup)
  {}
};

/** \brief event fired if an exception occured while creating B leg */
struct B2ABConnectOtherLegExceptionEvent: public B2ABEvent {
	
  unsigned int code;
  string reason;

  B2ABConnectOtherLegExceptionEvent(unsigned int code,
				    const string& reason)
    : B2ABEvent(B2ABConnectOtherLegException),
       code(code), reason(reason)
  {}
};

/** \brief event fired if the B leg could not be connected (e.g. busy) */
struct B2ABConnectOtherLegFailedEvent: public B2ABEvent {
	
  unsigned int code;
  string reason;

  B2ABConnectOtherLegFailedEvent(unsigned int code,
				 const string& reason)
    : B2ABEvent(B2ABConnectOtherLegFailed),
       code(code), reason(reason)
  {}
};
 
/**
 * \brief Base class for Sessions in B2ABUA mode.
 * 
 * It has two legs: Callee- and caller-leg.
 */
class AmB2ABSession: public AmSession
{	

 protected:
  /** local tag of the other leg */
  string other_id;
 
  /** Requests received for relaying */
  std::map<int,AmSipRequest> recvd_req;

  void clear_other();

  /** Relay one event to the other side. */
  virtual void relayEvent(AmEvent* ev);

  /** Terminate our leg and forget the other. */
  virtual void terminateLeg();

  /** Terminate the other leg and forget it.*/
  virtual void terminateOtherLeg();


  /** B2ABEvent handler */
  virtual void onB2ABEvent(B2ABEvent* ev);

  /*     // Other leg received a BYE */
  /*     virtual void onOtherBye(const AmSipRequest& req); */

  /*     /\** INVITE from other leg has been replied *\/ */
  /*     virtual void onOtherReply(const AmSipReply& reply); */

  /** @see AmEventQueue */
  void process(AmEvent* event);

  /** reference to the audio connector - caution: you may not delete this! */
  AmSessionAudioConnector* connector;

 public:

  AmB2ABSession();
  AmB2ABSession(const string& other_local_tag);
  ~AmB2ABSession();

  void onBye(const AmSipRequest& req);

  /** connect audio stream to the other session */
  void connectSession();

  /** reverse operation of connectSession */
  void disconnectSession();

};

class AmB2ABCalleeSession;

/** \brief Caller leg of a B2AB session */
class AmB2ABCallerSession: public AmB2ABSession
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

  virtual void setupCalleeSession(AmB2ABCalleeSession* callee_session,
				  B2ABConnectLegEvent* ev);
	
 protected:    
  virtual AmB2ABCalleeSession* createCalleeSession();
  void relayEvent(AmEvent* ev);

 public:
  AmB2ABCallerSession();
  ~AmB2ABCallerSession();

  CalleeStatus getCalleeStatus() { return callee_status; }

  void connectCallee(const string& remote_party,
		     const string& remote_uri,
		     const string& local_party,
		     const string& local_uri);

  // @see AmB2ABSession
  void terminateOtherLeg();

 protected:
  void onB2ABEvent(B2ABEvent* ev);
};

/** \brief Callee leg of a B2AB session */
class AmB2ABCalleeSession: public AmB2ABSession
{
 public:
  AmB2ABCalleeSession(const string& other_local_tag);
  ~AmB2ABCalleeSession();

  void onSessionStart(const AmSipReply& rep);
  void onSipReply(const AmSipReply& rep);	

 protected:
  void onB2ABEvent(B2ABEvent* ev);
};

/**
 * \brief \ref AmAudio that connects input and output with delay
 *
 *  AmAudioDelayBridge simply connects input and output 
 *  with a fixed packet size delay.
 */
class AmAudioDelayBridge : public AmAudio {
  SampleArrayShort sarr;
 public:
  AmAudioDelayBridge();
  ~AmAudioDelayBridge();
 protected:
  int write(unsigned int user_ts, unsigned int size);
  int read(unsigned int user_ts, unsigned int size);
};

/**
 * \brief connects the audio of two sessions together
 */
class AmSessionAudioConnector {

 private:
  AmAudioDelayBridge audio_connectors[2];
  string tag_sess[2];
  bool connected[2];
  AmMutex tag_mut;
  
 public:
  /** create a connector, connect audio to sess */
  AmSessionAudioConnector() { 
    connected[0] = false; 
    connected[1] = false;
  }

  ~AmSessionAudioConnector() {}

  /** connect audio to sess */
  void connectSession(AmSession* sess);

  /** disconnect session 
   * @return whether connector is still connected after disconnect 
   */
  bool disconnectSession(AmSession* sess);
};


#endif

