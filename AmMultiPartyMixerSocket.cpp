/*
 * $Id: AmMultiPartyMixerSocket.cpp,v 1.1.2.2 2005/06/01 12:00:24 rco Exp $
 *
 * Copyright (C) 2002-2003 Fhg Fokus
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

#include "AmMultiPartyMixerSocket.h"

#include "AmMultiPartyMixer.h"
#include "amci/codecs.h"

#include <assert.h>

int AmMultiPartyMixerSocket::write(unsigned int user_ts, unsigned int size)
{
//     audio_m.lock();
//     if(!audio_conf.empty()){
// 	DBG("...\n");
// 	AmAudio* audio = audio_conf.front();
// 	if(audio->get(user_ts,samples,size>>1) <= 0){
// 	    DBG("Audio source dropped !\n");
// 	    delete audio;
// 	    audio_conf.pop_front();
// 	    audio_m.unlock();
// 	    return 0;
// 	}
//     }
//     audio_m.unlock();

    mixer->PutChannelPacket(channel,user_ts,samples,size,begin_talk);
    return size;
}

int AmMultiPartyMixerSocket::read(unsigned int user_ts, unsigned int size)
{
    audio_m.lock();

    while (!audio_user.empty()){
	AmAudio* audio = audio_user.front();
	int s = audio->get(user_ts,samples,size>>1);
	if(s <= 0){
	    DBG("Audio source dropped !\n");
	    delete audio;
	    audio_user.pop_front();
//	    audio_m.unlock();
//	    return 0;
	} else {
	    audio_m.unlock();
	    return s;
	}
    }

    audio_m.unlock();

    mixer->GetChannelPacket(channel,user_ts,samples,size);
    return size;
}

AmMultiPartyMixerSocket::AmMultiPartyMixerSocket(AmMultiPartyMixer* mixer, Type type, 
					     unsigned int channel )
    : mixer(mixer), channel(channel), AmAudio(new AmAudioSimpleFormat(CODEC_PCM16))
{
}

AmMultiPartyMixerSocket::~AmMultiPartyMixerSocket()
{
    clearPlaylist(User);
    clearPlaylist(Conference);
}

void AmMultiPartyMixerSocket::addToPlaylist(AmAudio* src, PlayMode mode, Priority p)
{
    audio_m.lock();
    if(!src){
	audio_m.unlock();
	return;
    }

    deque<AmAudio*>* q = 0;
    switch(mode){
    case User:
	q = &audio_user;
	DBG("User Audio source added !\n");
	break;
//     case Conference:
// 	q = &audio_conf;
// 	DBG("Conference Audio source added !\n");
// 	break;
    default: 
	assert(0);
    }

//    src->fmt.reset(new AmAudioSimpleFormat(CODEC_PCM16));

    switch(p){
    case Q_Front:
	q->push_front(src);
	DBG("... to the Front \n");
	break;
    case Q_Back:
	q->push_back(src);
	DBG("... to the Back\n");
	break;
    default:
	assert(0);
    }
    audio_m.unlock();
}

void AmMultiPartyMixerSocket::clearPlaylist(PlayMode mode)
{
    audio_m.lock();
    switch(mode){
    case User:
	while(!audio_user.empty()){
	    DBG("User Audio source dropped !\n");
	    delete audio_user.front();
	    audio_user.pop_front();
	}
	break;
    case Conference:
	while(!audio_conf.empty()){
	    DBG("Conference Audio source dropped !\n");
	    delete audio_conf.front();
	    audio_conf.pop_front();
	}
	break;
    default:
	assert(0);
    }
    audio_m.unlock();
}
