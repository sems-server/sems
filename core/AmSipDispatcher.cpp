/*
 * $Id$
 *
 * Copyright (C) 2008 Raphael Coeffic
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

#include "AmSessionContainer.h"
#include "AmSipDialog.h"
#include "AmUtils.h"
#include "log.h"

#include "AmSipDispatcher.h"
#include "AmEventDispatcher.h"

AmSipDispatcher *AmSipDispatcher::_instance;

AmSipDispatcher* AmSipDispatcher::instance()
{
  return _instance ? _instance : ((_instance = new AmSipDispatcher()));
}

void AmSipDispatcher::handleSipMsg(AmSipReply &reply)
{
    AmSipReplyEvent* ev = new AmSipReplyEvent(reply);
    if(!AmEventDispatcher::instance()->post(reply.local_tag,ev)){
	
	ERROR("could not dispatch reply: %s\n", reply.print().c_str());
	delete ev;
    }
}

void AmSipDispatcher::handleSipMsg(AmSipRequest &req)
{
  string callid     = req.callid;
  string remote_tag = req.from_tag;
  string local_tag  = req.to_tag;

  AmEventDispatcher* ev_disp = AmEventDispatcher::instance();

  if(!local_tag.empty()) {
    AmSipRequestEvent* ev = new AmSipRequestEvent(req);

      if(!ev_disp->post(local_tag,ev)) {

	  delete ev;
	  
	  AmSipDialog::reply_error(req,481,
	      "Call leg/Transaction does not exist");
      }

      return;
  }

  if(ev_disp->postSipRequest(callid, remote_tag, req)){
	  
      return;
  }
  
  DBG("method: `%s' [%zd].\n", req.method.c_str(), req.method.length());
  
  if((req.method == "INVITE")){
      
      AmSessionContainer::instance()->startSessionUAS(req);
  }
  else if(req.method == "OPTIONS"){
      
      // Basic OPTIONS support
      AmSipDialog::reply_error(req,200,"OK");
      return;
  }
  else {

      AmSessionFactory* sess_fact = AmPlugIn::instance()->findSessionFactory(req);
      if(!sess_fact){

	  AmSipDialog::reply_error(req,500,"Not implemented");
	  return;
      }

      sess_fact->onOoDRequest(req);
  }
}
