/*
 * $Id: AmConference.cpp,v 1.1.2.4 2005/08/31 13:54:29 rco Exp $
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

#include "AmConference.h"
#include "AmConferenceStatus.h"
#include "AmSession.h"
#include "AmMultiPartyMixerSocket.h"

#include <utility>
using std::make_pair;
/* ----- audio mixer----------------------- */

AmConference* AmConference::_instance=0;

AmConference* AmConference::instance()
{
    if(!_instance)
	_instance = new AmConference();
    return _instance;
}

AmConference::AmConference() 
    : runcond(true)
{

}

AmConference::~AmConference() {

}

void AmConference::run() {
    while (runcond.get()) {
	// FIXME check sockets/conferences for inactive/single user ??? 
	sleep(5);
    }
}

void AmConference::on_stop() {

}

AmConference::socketpair* AmConference::find_socketpair(string c_id) {
    map<string, socketpair >::iterator it =  mixer_sockets.find(c_id); 
    if (it != mixer_sockets.end())
	return &(it->second);
    else 
	return 0;
}

int AmConference::conference_connect_sock(AmSession* session, string conf_id, bool send, bool receive, int options,
					  AmMultiPartyMixerSocket** play_sock, AmMultiPartyMixerSocket** rec_sock) {

    *play_sock = *rec_sock = 0;

    AmConferenceStatus* conf_status = AmConferenceStatusContainer::instance()->get_status(conf_id);

    mixer_sockets_mut.lock();
    // check if already connected to that conference
    if (find_socketpair(make_id(session, conf_id))) {
	mixer_sockets_mut.unlock();
	ERROR("already connected to conference '%s'\n", conf_id.c_str());
	return CONFERENCE_ERR_ALREADY_CONNECTED;
    }
    
    if (!conf_status) {
	if (!(options & CONFERENCE_OPTIONS_CREATE_CONF)) {
	    mixer_sockets_mut.unlock();
	    DBG("conference '%s' does not exist and shall not be created.\n", conf_id.c_str());
	    return CONFERENCE_ERR_NOTEXIST;
	}
	
	conf_status = AmConferenceStatusContainer::instance()->add_status(conf_id);
	if (!conf_status) {
	    mixer_sockets_mut.unlock();
	    ERROR("could not create new conference status\n");
	    return CONFERENCE_ERR_CREATESTATUS;
	}
    }

    unsigned int channel = conf_status->add_channel(session);

    if (send) 
	*play_sock = new AmMultiPartyMixerSocket(conf_status->get_mixer(), AmMultiPartyMixerSocket::Play, channel);
    if (receive) 
	*rec_sock = new AmMultiPartyMixerSocket(conf_status->get_mixer(), AmMultiPartyMixerSocket::Record, channel);

    mixer_sockets[make_id(session, conf_id)] = make_pair(*play_sock, *rec_sock);
    mixer_sockets_mut.unlock();
    
    return 0; // ok
}

// bool send : send to conference
// bool send : receive from conference (not rcv RTP!)
int AmConference::conference_connect(AmSession* session, string conf_id, bool send, bool receive, int options) 
{
    if (!send && !receive) {
	ERROR("connect must either send or receive or both.\n");
	return CONFERENCE_ERR_CONNTYPE;
    }
    
    AmMultiPartyMixerSocket* play_sock = 0;
    AmMultiPartyMixerSocket* rec_sock = 0; 
    
    if (int errcode = conference_connect_sock(session, conf_id, send, receive, options, &play_sock, &rec_sock))
	return errcode; // error
    
    if (send)
	session->setOutput(play_sock);
    if (receive)
	session->setInput(rec_sock);
    
    return 0;
}

void AmConference::conference_addToPlaylist(AmSession* session, string conf_id, 
					    AmAudio* src, AmMultiPartyMixerSocket::Type type, 
					    AmMultiPartyMixerSocket::PlayMode mode, 
					    AmMultiPartyMixerSocket::Priority priority) {

    if (!src) {
	ERROR("called with empty src!\n");
	return;
    }

    mixer_sockets_mut.lock();
    map<string, socketpair >::iterator it =  mixer_sockets.find(make_id(session, conf_id));
    if (it == mixer_sockets.end()) {
	mixer_sockets_mut.unlock();
	ERROR("session/conf %s not found in mixer socket list.\n", make_id(session, conf_id).c_str());
	return;
    }

    socketpair sockets = it->second;

    AmMultiPartyMixerSocket* dst_sock;
    if (type == AmMultiPartyMixerSocket::Play) 
	dst_sock = get_play_sock(sockets);
    else 
	dst_sock = get_rec_sock(sockets);

    dst_sock->addToPlaylist(src, mode, priority);
    mixer_sockets_mut.unlock();
}

void AmConference::conference_clearPlaylist(AmSession* session, string conf_id, 
					    AmMultiPartyMixerSocket::Type type, 
					    AmMultiPartyMixerSocket::PlayMode mode) {
    mixer_sockets_mut.lock();
    map<string, socketpair >::iterator it =  mixer_sockets.find(make_id(session, conf_id));
    if (it == mixer_sockets.end()) {
	mixer_sockets_mut.unlock();
	ERROR("session/conf %s not found in mixer socket list.\n", make_id(session, conf_id).c_str());
	return;
    }

    socketpair sockets = it->second;

    AmMultiPartyMixerSocket* dst_sock;
    if (type == AmMultiPartyMixerSocket::Play) 
	dst_sock = get_play_sock(sockets);
    else 
	dst_sock = get_rec_sock(sockets);

    dst_sock->clearPlaylist(mode);
    mixer_sockets_mut.unlock();
}

int AmConference::conference_disconnect(AmSession* session, string conf_id) {
    mixer_sockets_mut.lock();
    map<string, socketpair>::iterator it =  mixer_sockets.find(make_id(session, conf_id));
    if (it == mixer_sockets.end()) {
	mixer_sockets_mut.unlock();
	ERROR("session/conf %s not found in mixer socket list.\n", make_id(session, conf_id).c_str());
	return CONFERENCE_ERR_NOTEXIST;
    }

    socketpair sockets = it->second;

    if (get_play_sock(sockets)) 
	session->setOutput(0);
    if (get_rec_sock(sockets)) 
	session->setInput(0);

    AmConferenceStatus* conf_status = AmConferenceStatusContainer::instance()->get_status(conf_id);

//     unsigned int channel;
//     if (AmMultiPartyMixerSocket* s = get_play_sock(sockets)) {
// 	channel = s->getChannel();
//     } else if (AmMultiPartyMixerSocket* s = get_rec_sock(sockets)) {
// 	channel = s->getChannel();
//     }

    conf_status->remove_channel(session);

    if (get_play_sock(sockets))
	delete get_play_sock(sockets);
    if (get_rec_sock(sockets))
	delete get_rec_sock(sockets);

    mixer_sockets.erase(it); // erase it!
    mixer_sockets_mut.unlock();

    DBG("session 0x%lx disconnected from conference '%s'\n", (unsigned long) session, conf_id.c_str());
   return 0;
}

// disconnect sockets and destroy only, don't set session.input and session.output 
int AmConference::conference_disconnect_sock(AmSession* session, string conf_id) {
    mixer_sockets_mut.lock();

    map<string, socketpair>::iterator it =  mixer_sockets.find(make_id(session, conf_id));
    if (it == mixer_sockets.end()) {
	mixer_sockets_mut.unlock();
	ERROR("session/conf %s not found in mixer socket list.\n", make_id(session, conf_id).c_str());
	return CONFERENCE_ERR_NOTEXIST;
    }

    socketpair sockets = it->second;

    AmConferenceStatus* conf_status = AmConferenceStatusContainer::instance()->get_status(conf_id);

    conf_status->remove_channel(session);
    if (get_play_sock(sockets))
	delete get_play_sock(sockets);
    if (get_rec_sock(sockets))
	delete get_rec_sock(sockets);

    mixer_sockets.erase(it); // erase it!
    mixer_sockets_mut.unlock();

    DBG("session 0x%lx socket-disconnected from conference '%s'\n", (unsigned long) session, conf_id.c_str());
    return 0;
}

AmConference::socketpair AmConference::conference_get_sockets(AmSession* session, string conf_id) {
    mixer_sockets_mut.lock();
    map<string, socketpair>::iterator it =  mixer_sockets.find(make_id(session, conf_id));
    if (it == mixer_sockets.end()) {
	mixer_sockets_mut.unlock();
	ERROR("session/conf %s not found in mixer socket list.\n", make_id(session, conf_id).c_str());
	return make_pair((AmMultiPartyMixerSocket*)0,(AmMultiPartyMixerSocket*)0);
    }
    mixer_sockets_mut.unlock();
    return it->second;
}

string AmConference::make_id(const AmSession* session, const string& conf_id) {
    char sptr_chr[20];
    sprintf(sptr_chr, "%li", (unsigned long) session);
    return string(sptr_chr) + "/" + conf_id;
}

AmAudioQueue* AmConference::find_queue(string q_id) {
    map<string, AmAudioQueue*>::iterator it =  audio_queues.find(q_id);
    if (it != audio_queues.end())
	return it->second;
    else 
	return 0;
}

int AmConference::queue_connect(AmSession* session, string queue_id, bool send, bool receive) {
    if (!send && !receive) {
	ERROR("connect must either send or receive or both.\n");
	return CONFERENCE_ERR_CONNTYPE;
    }

    string q_id = make_id(session, queue_id);
    audio_queues_mut.lock();
    AmAudioQueue* q = find_queue(q_id);
    if (q) {
	audio_queues_mut.unlock();
	ERROR("already connected to queue with id '%s'\n", queue_id.c_str());
	return CONFERENCE_ERR_ALREADY_CONNECTED;
    }

    q = new AmAudioQueue();
    
    audio_queues[q_id] = q;
    audio_queue_connected[q_id] = make_pair(send, receive);

    if (send)
	session->setOutput(q);
    if (receive)
	session->setInput(q);

    audio_queues_mut.unlock();    
    return 0;
}

int AmConference::queue_disconnect(AmSession* session, string queue_id) {
    string q_id = make_id(session, queue_id);
    audio_queues_mut.lock();
    AmAudioQueue* q = find_queue(q_id);
    if (!q) {
	audio_queues_mut.unlock();
	ERROR("queue with id '%s' not found\n", queue_id.c_str());
	return CONFERENCE_ERR_NOTEXIST;
    }
    
    if (audio_queue_connected[q_id].first)
	session->setOutput(0);
    if (audio_queue_connected[q_id].second)
	session->setInput(0);

    audio_queues.erase(audio_queues.find(q_id));
    audio_queue_connected.erase(audio_queue_connected.find(q_id));
    audio_queues_mut.unlock();

    delete q; // delete the queue itself

    return 0;
}

int AmConference::queue_pushAudio(AmSession* session, string queue_id, AmAudio* audio, AmAudioQueue::QueueType type, 
				  AmAudioQueue::Pos pos, bool write, bool read) {
    if (!audio) {
	ERROR("not a valid audio source\n");
	return CONFERENCE_ERR_INVALID_AUDIO;
    }
	
    string q_id = make_id(session, queue_id);
    audio_queues_mut.lock();
    AmAudioQueue* q = find_queue(q_id);
    if (!q) {
	audio_queues_mut.unlock();
	ERROR("queue with id '%s' not found\n", queue_id.c_str());
	return CONFERENCE_ERR_NOTEXIST;
    }

    q->pushAudio(audio, type, pos, write, read);
    audio_queues_mut.unlock();    
    return 0;
}

int AmConference::queue_popAudio(AmSession* session, string queue_id, AmAudioQueue::QueueType type,
				 AmAudioQueue::Pos pos) {
    string q_id = make_id(session, queue_id);
    audio_queues_mut.lock();
    AmAudioQueue* q = find_queue(q_id);
    if (!q) {
	audio_queues_mut.unlock();
	ERROR("queue with id '%s' not found\n", queue_id.c_str());
	return CONFERENCE_ERR_NOTEXIST;
    }

    int res = q->popAudio(type, pos);
    audio_queues_mut.unlock();    
    return res;
}

AmAudio* AmConference::queue_popAndGetAudio(AmSession* session, string queue_id, AmAudioQueue::QueueType type,
					    AmAudioQueue::Pos pos) {
    string q_id = make_id(session, queue_id);
    audio_queues_mut.lock();
    AmAudioQueue* q = find_queue(q_id);
    if (!q) {
	audio_queues_mut.unlock();
	ERROR("queue with id '%s' not found\n", queue_id.c_str());
	return 0; //CONFERENCE_ERR_NOTEXIST;
    }

    AmAudio* res = q->popAndGetAudio(type, pos);
    audio_queues_mut.unlock();    

    return res;
}

int AmConference::queue_pushConference(AmSession* session, string queue_id, string conf_id, 
				       bool write, bool read, 
				       AmAudioQueue::Pos input_queue_pos, AmAudioQueue::Pos output_queue_pos,
				       int options) {
    string q_id = make_id(session, queue_id);
    audio_queues_mut.lock();
    AmAudioQueue* q = find_queue(q_id);
    if (!q) {
	audio_queues_mut.unlock();
	ERROR("queue with id '%s' not found\n", queue_id.c_str());
	return CONFERENCE_ERR_NOTEXIST;
    }

    AmMultiPartyMixerSocket* play_sock; 
    AmMultiPartyMixerSocket* rec_sock;

    int res = conference_connect_sock(session, conf_id, write, read, options,
				      &play_sock, &rec_sock);
    if (res) {
	audio_queues_mut.unlock();
	return res;
    }
					  
    if (write) {
	// we should have the play_sock now
	q->pushAudio(play_sock, AmAudioQueue::InputQueue, input_queue_pos, true, false);
    }

    if (read) {
	// we should have the rec_sock
	q->pushAudio(rec_sock, AmAudioQueue::OutputQueue, output_queue_pos, false, true);
    }

    audio_queues_mut.unlock();    
    return 0;
}

int AmConference::queue_removeConference(AmSession* session, string queue_id, string conf_id) {
    string q_id = make_id(session, queue_id);

    audio_queues_mut.lock();
    AmAudioQueue* q = find_queue(q_id);
    if (!q) {
	audio_queues_mut.unlock();
	ERROR("queue with id '%s' not found\n", queue_id.c_str());
	return CONFERENCE_ERR_NOTEXIST;
    }
    socketpair conf_sockets = conference_get_sockets(session, conf_id);

    if ((!get_play_sock(conf_sockets))&&(!get_rec_sock(conf_sockets))) {
	audio_queues_mut.unlock();
	ERROR("conference with id '%s' is not connected here\n", conf_id.c_str());
	return CONFERENCE_ERR_NOTEXIST;
    }

    q->removeAudio(get_play_sock(conf_sockets));
    q->removeAudio(get_rec_sock(conf_sockets));

    int res = conference_disconnect_sock(session, conf_id);

    audio_queues_mut.unlock();
    DBG("removed conference from queue\n");
    return res;
}

int AmConference::queue_pushBackConference(AmSession* session, string queue_id, string conf_id) {
    return queue_pushConference(session, queue_id, conf_id, 
				true, true, AmAudioQueue::Back, AmAudioQueue::Front,
				CONFERENCE_OPTIONS_CREATE_CONF);
}
    
