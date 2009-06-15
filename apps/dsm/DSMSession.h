/*
 * $Id$
 *
 * Copyright (C) 2008 iptego GmbH
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
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

#define DSM_ERRNO_FILE        "1"
#define DSM_ERRNO_UNKNOWN_ARG "2"
#define DSM_ERRNO_GENERAL     "99"
#define DSM_ERRNO_OK          ""

#define DSM_REPLY_REQUEST      "reply_request" // todo: rethink these names
#define DSM_REPLY_REQUEST_FALSE "0"

#define DSM_CONNECT_SESSION    "connect_session" // todo: rethink these names
#define DSM_CONNECT_SESSION_FALSE    "0"

#define SET_ERRNO(new_errno) \
    var["errno"] = new_errno

class DSMDisposable;
class AmPlaylistItem;

class DSMSession {

 public:
  DSMSession();
  virtual ~DSMSession();

  virtual void playPrompt(const string& name, bool loop = false) = 0;
  virtual void playFile(const string& name, bool loop, bool front = false) = 0;
  virtual void recordFile(const string& name) = 0;
  virtual unsigned int getRecordLength() = 0;
  virtual unsigned int getRecordDataSize() = 0;
  virtual void stopRecord() = 0;
  virtual void addToPlaylist(AmPlaylistItem* item) = 0;
  virtual void closePlaylist(bool notify) = 0;
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

  /** insert reqeust in list of received ones */
  virtual void B2BaddReceivedRequest(const AmSipRequest& req) = 0;

  /** transfer ownership of object to this session instance */
  virtual void transferOwnership(DSMDisposable* d) = 0;

  /* holds variables which are accessed by $varname */
  map<string, string> var;

  /* holds AmArg variables. todo: merge var with these */
  map<string, AmArg> avar;

  /* result of the last DI call */
  AmArg di_res;

  /* last received request */
  std::auto_ptr<AmSipRequest> last_req;
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

#define DSM_EVENT_ID -10
/**  generic event for passing events between DSM sessions */
struct DSMEvent : public AmEvent {
 DSMEvent() : AmEvent(DSM_EVENT_ID) { }
  ~DSMEvent() { }
  map<string, string> params;
};

#endif
