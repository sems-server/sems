/*
 * $Id: AmSimpleMWI.h,v 1.2.2.1 2005/09/02 13:47:46 rco Exp $
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

#ifndef AMSIMPLEMWI_H
#define AMSIMPLEMWI_H

#include "AmSIPEventDaemon.h"
#include <string>
#include <map>
#include "log.h"

/**
 * RFC3842 style Message waiting counters
 */

struct  AmMessageWaitingCount {
    int n_new;    // new messages 
    int n_old;    // old msgs

    bool urgent;  // do we have urgent messages indication?

    int urgent_new;
    int urgent_old;
    AmMessageWaitingCount() 
	: n_new(0), n_old(0), urgent(false) { }

    AmMessageWaitingCount(int new_, int old_, 
			bool urgent_ = false, 
			int urgent_new_ = 0,
			int urgent_old_ = 0) 
	: n_new(new_), n_old(old_), urgent(urgent_), 
    urgent_new(urgent_new_), urgent_old(urgent_old_) { }
    
    bool operator!=(const AmMessageWaitingCount& c) const {
	return (c.n_new != n_new) 
	    || (c.n_old != n_old)
	    || (c.urgent != urgent)
	    || (urgent && 
		((c.urgent_new != urgent_new) || 
		 (c.urgent_old != urgent_old))); 
    }
};

/**
 * RFC3842 style Message waiting indication
 */
class AmSimpleMailboxState : public AmResourceState 
{
    bool mwi;
    std::string mwi_state;
    std::map<std::string, AmMessageWaitingCount> messages;   // message class name and counts

public:
    AmSimpleMailboxState() 
	: AmResourceState () { } 

    AmSimpleMailboxState(bool have_messages) 
	: AmResourceState(), mwi(have_messages)
	{ } 

    AmSimpleMailboxState(bool have_messages, 
		       std::map<std::string, AmMessageWaitingCount> m_counts, 
		       const std::string& summary = "") 
	: AmResourceState(), mwi(have_messages), 
	messages(m_counts), mwi_state(summary)
	{ } 


    std::string getState(const AmSubscription* subs, std::string& content_type);

    bool setState(AmResourceState* new_state);

    void setContent(bool have_messages);
    void setContent(bool have_messages, std::map<std::string, AmMessageWaitingCount> m_counts, 
		    const std::string& summary = "");
};

#endif
