/*
 * $Id: AmRtpScheduler.cpp,v 1.3 2003/11/25 16:01:39 rco Exp $
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

#include "AmRtpScheduler.h"
#include "log.h"

#include <signal.h>
#include <sys/time.h>

bool operator < (const AmRtpTimer& l, const AmRtpTimer& r)
{
    return timercmp(&l.time,&r.time,<);
}

AmRtpScheduler* AmRtpScheduler::_instance=0;

AmRtpScheduler* AmRtpScheduler::instance()
{
    if(!_instance)
	_instance = new AmRtpScheduler();

    return _instance;
}

AmRtpScheduler::AmRtpScheduler()
{
    gettimeofday(&begin_time,NULL);
}

AmRtpScheduler::~AmRtpScheduler()
{
}

AmSharedVar<bool> alarm_fired;

void h_alarm(int signum)
{
    alarm_fired.set(true);
}

void AmRtpScheduler::run()
{
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset,SIGALRM);
    sigprocmask(SIG_BLOCK,&sigset,NULL);

    struct itimerval tval;
    tval.it_value.tv_sec = 0;
    tval.it_value.tv_usec = 10000; // 10ms
    tval.it_interval = tval.it_value;
    setitimer(ITIMER_REAL,&tval,NULL);

    sigfillset(&sigset);
    sigdelset(&sigset,SIGALRM);

    while(true){

	alarm_fired.set(false);
	signal(SIGALRM,h_alarm);

	while(!alarm_fired.get()) 
	    sigsuspend(&sigset);

	struct timeval cur_time = getCurrentTime();
	timers_mut.lock();
	if(timers.empty()){
	    timers_mut.unlock();
	    continue;
	}

	set<AmRtpTimer>::iterator it = timers.begin();

	while( timercmp(&it->time,&cur_time,<) 
	       || timercmp(&it->time,&cur_time,==) ){

	    AmCondition<bool>* fired = it->fired;
	    timers.erase(it);
	    fired->set(true);

	    if(timers.empty()) break;
	    it = timers.begin();
	}

	timers_mut.unlock();
    }
}

void AmRtpScheduler::on_stop()
{
}

void AmRtpScheduler::addTimer(const AmRtpTimer& timer)
{
    timers_mut.lock();
    timers.insert(timer);
    timers_mut.unlock();
}

struct timeval AmRtpScheduler::getCurrentTime()
{
    struct timeval tval;
    gettimeofday(&tval,NULL);
    timersub(&tval,&begin_time,&tval);
    return tval;
}
