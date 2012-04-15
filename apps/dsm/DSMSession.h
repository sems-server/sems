/*
 * Copyright (C) 2008 iptego GmbH
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
#ifndef _DSM_SESSION_H
#define _DSM_SESSION_H

#include "AmArg.h"
#include "AmEvent.h"
#include "AmSipMsg.h"
#include "AmAudioFile.h"

#include <string>
using std::string;
#include <vector>
using std::vector;
#include <map>
using std::map;
#include <memory>

#define DSM_TRUE "true"
#define DSM_FALSE "false"

#define DSM_PROCESSED "processed"
#define DSM_CONNECT   "connect"

#define DSM_CONNECT_SESSION    "connect_session" // todo: rethink these names
#define DSM_CONNECT_SESSION_FALSE    "0"

#define DSM_ACCEPT_EARLY_SESSION    "accept_early_session" // todo: rethink these names
#define DSM_ACCEPT_EARLY_SESSION_FALSE   "0"

#define DSM_CONNECT_EARLY_SESSION        "connect_early_session" // todo: rethink these names
#define DSM_CONNECT_EARLY_SESSION_FALSE    "0"

#define DSM_ENABLE_REQUEST_EVENTS  "enable_request_events"
#define DSM_ENABLE_REPLY_EVENTS    "enable_reply_events"


#define DSM_AVAR_REQUEST "request"
#define DSM_AVAR_REPLY   "reply"

#define DSM_AVAR_JSONRPCREQUESTDATA "JsonRpcRequestParameters"
#define DSM_AVAR_JSONRPCRESPONSEDATA "JsonRpcResponseParameters"
#define DSM_AVAR_JSONRPCRESPONSEUDATA "JsonRpcResponseUData"

#define DSM_AVAR_SIPSUBSCRIPTION_BODY "SipSubscriptionBody"

#define DSM_ERRNO_FILE        "file"
#define DSM_ERRNO_UNKNOWN_ARG "arg"
#define DSM_ERRNO_SCRIPT      "script"
#define DSM_ERRNO_CONFIG      "config"
#define DSM_ERRNO_INTERNAL    "internal"
#define DSM_ERRNO_GENERAL     "general"


#define DSM_ERRNO_OK          ""

#define SET_ERRNO(new_errno)			\
  var["errno"] = new_errno

#define CLR_ERRNO				\
  var["errno"] = DSM_ERRNO_OK;

#define SET_STRERROR(new_str)			\
  var["strerror"] = new_str

#define CLR_STRERROR				\
  var["strerror"] = "";

typedef map<string, string> VarMapT;
typedef map<string, AmArg>  AVarMapT;

class DSMDisposable;
struct AmPlaylistItem;

class DSMSession {

 public:
  DSMSession();
  virtual ~DSMSession();

  virtual void playPrompt(const string& name, bool loop = false, bool front = false) = 0;
  virtual void playFile(const string& name, bool loop, bool front = false) = 0;
  virtual void playSilence(unsigned int length, bool front = false) = 0;
  virtual void recordFile(const string& name) = 0;
  virtual unsigned int getRecordLength() = 0;
  virtual unsigned int getRecordDataSize() = 0;
  virtual void stopRecord() = 0;
  virtual void setInOutPlaylist() = 0;
  virtual void setInputPlaylist() = 0;
  virtual void setOutputPlaylist() = 0;

  virtual void addToPlaylist(AmPlaylistItem* item, bool front = false) = 0;
  virtual void flushPlaylist() = 0;
  virtual void setPromptSet(const string& name) = 0;
  virtual void addSeparator(const string& name, bool front = false) = 0;
  virtual void connectMedia() = 0;
  virtual void disconnectMedia() = 0;
  virtual void mute() = 0;
  virtual void unmute() = 0;

  /** B2BUA functions */
  virtual void B2BconnectCallee(const string& remote_party,
				const string& remote_uri,
				bool relayed_invite = false) = 0;
  virtual void B2BterminateOtherLeg() = 0;

  /** insert request in list of received ones */
  virtual void B2BaddReceivedRequest(const AmSipRequest& req) = 0;

  /** set headers of outgoing INVITE */
  virtual void B2BsetHeaders(const string& hdr, bool replaceCRLF) = 0;

  /** set headers of outgoing INVITE */
  virtual void B2BclearHeaders() = 0;

  /** add a header to the headers of outgoing INVITE */
  virtual void B2BaddHeader(const string& hdr) = 0;

  /** transfer ownership of object to this session instance */
  virtual void transferOwnership(DSMDisposable* d) = 0;

  /** release ownership of object from this session instance */
  virtual void releaseOwnership(DSMDisposable* d) = 0;

  /* holds variables which are accessed by $varname */
  VarMapT var;

  /* holds AmArg variables. todo(?): merge var with these */
  AVarMapT avar;

  /* result of the last DI call */
  AmArg di_res;

  /* last received request */
  std::auto_ptr<AmSipRequest> last_req;
};

class DSMStateDiagramCollection;

struct DSMScriptConfig {
  DSMStateDiagramCollection* diags;
  map<string,string> config_vars;

  bool RunInviteEvent;
  bool SetParamVariables;
};

class DSMDisposable {
 public:
  DSMDisposable() { }
  virtual ~DSMDisposable() { }
};

class DSMDisposableAudioFile 
: public DSMDisposable, 
  public AmAudioFile 
{
 public:
  DSMDisposableAudioFile() { }
  ~DSMDisposableAudioFile() { }
};

class DSMSipRequest
: public AmObject {
 public: 
  const AmSipRequest* req;

  DSMSipRequest(const AmSipRequest* req)
    : req(req)  { }
  ~DSMSipRequest() { }
};

class DSMSipReply
: public AmObject {
 public: 
  const AmSipReply* reply;

  DSMSipReply(const AmSipReply* reply)
    : reply(reply)  { }
  ~DSMSipReply() { }
};


#define DSM_EVENT_ID -10
/**  generic event for passing events between DSM sessions */
struct DSMEvent : public AmEvent {
 DSMEvent() : AmEvent(DSM_EVENT_ID) { }
  ~DSMEvent() { }
  map<string, string> params;
};

#endif
