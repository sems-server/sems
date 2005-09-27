/*
 * $Id: AmRtpScheduler.h,v 1.3 2003/11/25 16:03:10 rco Exp $
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

#ifndef _AmRtpScheduler_h_
#define _AmRtpScheduler_h_

#include "AmThread.h"

#include <set>
using std::set;

struct AmRtpTimer
{
    AmCondition<bool>* fired;
    struct timeval     time;

    AmRtpTimer():fired(0){}

    AmRtpTimer(AmCondition<bool>* fired)
	: fired(fired){}

    AmRtpTimer(const  AmRtpTimer& t)
	: fired(t.fired), time(t.time){}
};

bool operator < (const AmRtpTimer& l, const AmRtpTimer& r);

class AmRtpScheduler: public AmThread {

    static AmRtpScheduler* _instance;

    struct timeval  begin_time;
    set<AmRtpTimer> timers;
    AmMutex         timers_mut;

    AmRtpScheduler();
    ~AmRtpScheduler();

    void run();
    void on_stop();

public:
    static AmRtpScheduler* instance();
    void addTimer(const AmRtpTimer& timer);
    struct timeval getCurrentTime();
};

#endif

// Local Variables:
// mode:C++
// End:
