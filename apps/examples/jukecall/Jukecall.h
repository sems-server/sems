/*
 * Copyright (C) 2006 ipteo GmbH
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For a license to use the sems software under conditions
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

#ifndef _JUKECALL_H_
#define _JUKECALL_H_

#include "AmB2ABSession.h"
#include "AmConfigReader.h"
#include "AmAudioFile.h"
#include "AmApi.h"

#include <string>
using std::string;

class JukecallFactory: public AmSessionFactory
{
public:
  JukecallFactory(const string& _app_name);

  int onLoad();

  AmSession* onInvite(const AmSipRequest& req, const string& app_name,
		      const map<string,string>& app_params);
};

class JukecallSession 
  : public AmB2ABCallerSession
{

public:
  enum JukeLeg1_state {
    JC_none = 0,
    JC_initial_announcement,
    JC_connect,
    JC_juke
  };

private:
  // our state
  JukeLeg1_state state;
  AmAudioFile initial_announcement;

  auto_ptr<AmAudioFile> song;

protected:
  AmB2ABCalleeSession* createCalleeSession();

public:

  JukecallSession();
  ~JukecallSession();

  void onSessionStart();
  void process(AmEvent* event);

  void onDtmf(int event, int duration_msec);
};

class JukecalleeSession 
  : public AmB2ABCalleeSession {
	
  void process(AmEvent* event);
  auto_ptr<AmAudioFile> song;

public:
  JukecalleeSession(const string& other_tag, 
		    AmSessionAudioConnector* connector);
};
 
class JukeEvent : public AmEvent {
public:
  JukeEvent(int key) 
    : AmEvent(key) {}
};

#endif
// Local Variables:
// mode:C++
// End:

