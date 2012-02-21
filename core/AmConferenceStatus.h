/*
 * Copyright (C) 2002-2003 Fhg Fokus
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
/** @file AmConferenceStatus.h */
#ifndef _ConferenceStatus_h_
#define _ConferenceStatus_h_

#include "AmThread.h"
#include "AmRtpStream.h"
#include "AmMultiPartyMixer.h"
#include "AmEventQueue.h"

#include <map>
#include <string>
using std::string;

class AmConferenceChannel;

enum { ConfNewParticipant = 1,
       ConfParticipantLeft };

/** \brief event in a conference*/
struct ConferenceEvent: public AmEvent
{
  unsigned int participants;
  string       conf_id;
  string       sess_id;

  ConferenceEvent(int event_id, 
		  unsigned int participants,
		  const string& conf_id,
		  const string& sess_id)
    : AmEvent(event_id),
      participants(participants),
      conf_id(conf_id),
      sess_id(sess_id)
  {}
};

/**
 * \brief One conference (room).
 * 
 * The ConferenceStatus manages one conference.
 */
class AmConferenceStatus
{
  static std::map<string,AmConferenceStatus*> cid2status;
  static AmMutex                         cid2s_mut;

  struct SessInfo {

    string       sess_id;
    unsigned int ch_id;

    SessInfo(const string& local_tag,
	     unsigned int  ch_id)
      : sess_id(local_tag),
	ch_id(ch_id)
    {}
  };

  string                 conf_id;
  AmMultiPartyMixer      mixer;
    
  // sess_id -> ch_id
  std::map<string, unsigned int> sessions;

  // ch_id -> sess_id
  std::map<unsigned int, SessInfo*> channels;

  AmMutex                      sessions_mut;

  AmConferenceStatus(const string& conference_id);
  ~AmConferenceStatus();

  AmConferenceChannel* getChannel(const string& sess_id, int input_sample_rate);

  int releaseChannel(unsigned int ch_id);

  void postConferenceEvent(int event_id, const string& sess_id);

public:
  const string&      getConfID() { return conf_id; }
  AmMultiPartyMixer* getMixer()  { return &mixer; }

  static AmConferenceChannel* getChannel(const string& cid, 
					 const string& local_tag,
                                         int input_sample_rate);

  static void releaseChannel(const string& cid, unsigned int ch_id);

  static void postConferenceEvent(const string& cid, int event_id, 
				  const string& sess_id);

  static size_t getConferenceSize(const string& cid);
};

#endif
// Local Variables:
// mode:C++
// End:

