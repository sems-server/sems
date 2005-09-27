/*
 * $Id: AmMultiPartyMixerSocket.h,v 1.1.2.1 2005/03/07 21:34:45 sayer Exp $
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

#ifndef _MultiPartyMixerSocket_h_
#define _MultiPartyMixerSocket_h_

#include "AmAudio.h"
#include "AmThread.h"

#include <deque>
using std::deque;

class AmMultiPartyMixer;

class AmMultiPartyMixerSocket: public AmAudio
{
    deque<AmAudio*> audio_user;
    deque<AmAudio*> audio_conf;
    AmMutex         audio_m;

    AmMultiPartyMixer*  mixer;
    unsigned int      channel;

public:

    enum Type { Play=0, Record };
    enum PlayMode { User=0, Conference };
    enum Priority { Q_Front=0, Q_Back };

    /**
     * @param mixer  multi-party mixer
     * @param fmt    in- or output format, depending on the mode
     */
    AmMultiPartyMixerSocket( AmMultiPartyMixer* mixer, Type type, 
			   unsigned int channel );

    ~AmMultiPartyMixerSocket();

    /** inserts an alternative audio source to play_mode */
    void addToPlaylist(AmAudio* src, PlayMode mode, Priority p);

    /** clears the alternative audio playlist */
    void clearPlaylist(PlayMode mode);

    unsigned int getChannel() { return channel; }

protected:

    int write(unsigned int user_ts, unsigned int size);
    int read(unsigned int user_ts, unsigned int size);
};

#endif
