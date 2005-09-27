/*
 * $Id: AmConference.h,v 1.1.2.2 2005/03/14 20:58:23 sayer Exp $
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
#ifndef AM_CONFERENCE_H
#define AM_CONFERENCE_H

#include "AmThread.h"
#include "AmAudio.h"
#include "AmMultiPartyMixerSocket.h"
#include "AmMultiPartyMixer.h"

#include "AmAdvancedAudio.h"
#include <utility>
#include <map>
using std::map;
using std::pair;

#define CONFERENCE_OPTIONS_CREATE_CONF   0x00000001

#define CONFERENCE_ERR_CONNTYPE          -1 
#define CONFERENCE_ERR_NOTEXIST          -2
#define CONFERENCE_ERR_CREATESTATUS      -3 
#define CONFERENCE_ERR_ALREADY_CONNECTED -4 
#define CONFERENCE_ERR_INVALID_AUDIO     -5

// silly, but makes source clearer
#define CONF_DO_SEND true
#define CONF_DONT_SEND false
#define CONF_DO_RECEIVE true
#define CONF_DONT_RECEIVE false
#define QUEUE_DO_WRITE true
#define QUEUE_DONT_WRITE false
#define QUEUE_DO_READ true
#define QUEUE_DONT_READ false

class AmSession;

/* ----- conference mixer: handler and api ----- */
class AmConference : public AmThread {
 private:
    static AmConference* _instance;
    AmSharedVar<bool> runcond;

    typedef pair<AmMultiPartyMixerSocket*, AmMultiPartyMixerSocket*> socketpair;
#define get_play_sock(p) p.first 
#define get_rec_sock(p) p.second 
    map<string, socketpair > mixer_sockets; 
    AmMutex mixer_sockets_mut;

    map<string, AmAudioQueue*> audio_queues;
    map<string, pair<bool, bool> > audio_queue_connected;
    AmMutex audio_queues_mut;

    string make_id(const AmSession* session, const string& conf_id);

    AmAudioQueue* find_queue(string q_id);
    socketpair* find_socketpair(string c_id);

    // connect and return pointers to the sockets
    int conference_connect_sock(AmSession* session, string conf_id, bool send, bool receive, int options,
				AmMultiPartyMixerSocket** play_sock, AmMultiPartyMixerSocket** rec_sock);
    int conference_disconnect_sock(AmSession* session, string conf_id);
    socketpair conference_get_sockets(AmSession* session, string conf_id);

 protected:
    void run();
    void on_stop();

 public:
    AmConference();
    ~AmConference();

    static  AmConference* instance();

/** simple conference functions */    
    int conference_connect(AmSession* session, string conf_id, bool send, bool receive, int options);
//  void conference_change_connection(AmSession* session, string conf_id,  bool send, bool receive);
    int conference_disconnect(AmSession* session, string conf_id);


    void conference_addToPlaylist(AmSession* session, string conf_id, 
		       AmAudio* src, AmMultiPartyMixerSocket::Type type, 
		       AmMultiPartyMixerSocket::PlayMode mode, 
		       AmMultiPartyMixerSocket::Priority priority);
    void conference_clearPlaylist(AmSession* session, string conf_id, 
		       AmMultiPartyMixerSocket::Type type, 
		       AmMultiPartyMixerSocket::PlayMode mode);
    // FIXME: implement conference status info (participants count etc)

/** advanced conference with audioqueue functions */
    int queue_connect(AmSession* session, string queue_id, bool send, bool receive);
    int queue_disconnect(AmSession* session, string queue_id);
//    int queue__change_connection(AmSession* session, string queue_id, bool send, bool receive);
    int queue_pushAudio(AmSession* session, string queue_id, AmAudio* audio, AmAudioQueue::QueueType type, 
			AmAudioQueue::Pos pos, bool write, bool read);
    int queue_popAudio(AmSession* session, string queue_id, AmAudioQueue::QueueType type,
			AmAudioQueue::Pos pos);
    AmAudio* queue_popAndGetAudio(AmSession* session, string queue_id, AmAudioQueue::QueueType type,
				  AmAudioQueue::Pos pos);
    int queue_pushConference(AmSession* session, string queue_id, string conf_id, 
			     bool write, bool read, 
			     AmAudioQueue::Pos input_queue_pos, AmAudioQueue::Pos output_queue_pos,
			     int options);
    int queue_removeConference(AmSession* session, string queue_id, string conf_id);

    int queue_pushBackConference(AmSession* session, string queue_id, string conf_id);
};

#endif //AM_CONFERENCE_H
