/*
 * $Id: AmSimpleMWI.cpp,v 1.3.2.1 2005/09/02 13:47:46 rco Exp $
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
#include "AmSimpleMWI.h"
#include "log.h"
#include "AmUtils.h"

using std::string;
using std::map;

string AmSimpleMailboxState::getState(const AmSubscription* subs, string& content_type) {
    content_type = "application/simple-message-summary";
	
    DBG("building body...\n");
    if (mwi) {
	string mwi_body = "Messages-Waiting: yes\n";
	
	if (messages.size()) {
	    DBG("messages.size = %ld\n", (long)messages.size());
	    for (map<string, AmMessageWaitingCount>::iterator it = messages.begin(); 
		 it != messages.end(); it++) {
		mwi_body += it->first + ": "+int2str(it->second.n_new)+"/"+int2str(it->second.n_old);
		if (it->second.urgent) {
		    mwi_body += " ("+int2str(it->second.urgent_new)+"/"+int2str(it->second.urgent_old)+")";
		}
		mwi_body += "\n";
	    }
	}

	if (mwi_state.length()) {
	   mwi_body += "\n"+mwi_state;
	}  
	return mwi_body;
    } else 
	return "Messages-Waiting: no\n";
}

bool AmSimpleMailboxState::setState(AmResourceState* new_state) {
    AmSimpleMailboxState* s  = dynamic_cast<AmSimpleMailboxState*>(new_state);
    if (s) {
	bool ret = (s->mwi_state != mwi_state) 
	    || (s->mwi != mwi) 
	    || (s->messages.size() != messages.size()) 
	    || (s->mwi_state.size() != mwi_state.size()) ;
	
	if (!ret) { // check counts
	    for (map<const string, AmMessageWaitingCount>::iterator it = s->messages.begin(); 
		 it != s->messages.end(); it++) {
		
		map<string, AmMessageWaitingCount>::iterator t_it = messages.find(it->first);
		if ( (t_it == messages.end()) || (it->second != t_it->second) ) { // new category or not equal counts
		    ret = true;
		    break;
		}
	    }
	}

	if (ret) {
	    mwi_state = s->mwi_state;
	    messages = s->messages;
	    mwi = s->mwi;
	}
	return ret;
    } else {
	DBG("Error: got wrong resource state.\n");
	return true;
    }
}

void AmSimpleMailboxState::setContent(bool have_messages) {
   mwi = have_messages;
}

void AmSimpleMailboxState::setContent(bool have_messages, 
				      map<string, AmMessageWaitingCount> m_counts, 
				      const string& summary) {
   mwi = have_messages;
   mwi_state = summary;
   messages = m_counts;
}

