/*
 * Copyright (C) 2008 Raphael Coeffic
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. This program is released under
 * the GPL with the additional exemption that compiling, linking,
 * and/or using OpenSSL is allowed.
 *
 * For a license to use the SEMS software under conditions
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

void AmSipDispatcher::handleSipMsg(const string& dialog_id, AmSipReply &reply)
{
  const string& id = dialog_id.empty() ? reply.from_tag : dialog_id;
  AmSipReplyEvent* ev = new AmSipReplyEvent(reply);

  if(!AmEventDispatcher::instance()->post(id,ev)){
    if ((reply.code >= 100) && (reply.code < 300)) {
      if (AmConfig::UnhandledReplyLoglevel >= 0) {
	_LOG(AmConfig::UnhandledReplyLoglevel,
	     "unhandled SIP reply: %s\n", reply.print().c_str());
      }
    } else {
      WARN("unhandled SIP reply: %s\n", reply.print().c_str());
    }
    delete ev;
  }
}

void AmSipDispatcher::handleSipMsg(AmSipRequest &req)
{
  string callid     = req.callid;
  string remote_tag = req.from_tag;
  string local_tag  = req.to_tag;

  AmEventDispatcher* ev_disp = AmEventDispatcher::instance();

  if(req.method == SIP_METH_CANCEL){
      
    if(ev_disp->postSipRequest(req)){
      return;
    }
  
    // CANCEL of a (here) non-existing dialog
    AmSipDialog::reply_error(req,481,SIP_REPLY_NOT_EXIST);
    return;
  } 
  else if(!local_tag.empty()) {
    // in-dlg request
    AmSipRequestEvent* ev = new AmSipRequestEvent(req);

    // Contact-user may contain internal dialog ID (must be tried before using
    // local_tag for identification)
    if(!req.user.empty() && ev_disp->post(req.user,ev))
      return;

    if(ev_disp->post(local_tag,ev))
      return;

    delete ev;
    if(req.method != SIP_METH_ACK) {
      AmSipDialog::reply_error(req,481,
			       "Call leg/Transaction does not exist");
    }
    else {
      DBG("received ACK for non-existing dialog "
	  "(callid=%s;remote_tag=%s;local_tag=%s)\n",
	  callid.c_str(),remote_tag.c_str(),local_tag.c_str());
    }

    return;
  }

  DBG("method: `%s' [%zd].\n", req.method.c_str(), req.method.length());
  if(req.method == SIP_METH_INVITE){
      
      AmSessionContainer::instance()->startSessionUAS(req);
  }
  else if(req.method == SIP_METH_BYE ||
	  req.method == SIP_METH_PRACK){
    
    // BYE/PRACK of a (here) non-existing dialog
    AmSipDialog::reply_error(req,481,SIP_REPLY_NOT_EXIST);
    return;

  } else {

    string app_name;
    AmSessionFactory* sess_fact = AmPlugIn::instance()->findSessionFactory(req,app_name);
    if (sess_fact) {
      sess_fact->onOoDRequest(req);
      return;
    }
	
    if (req.method == SIP_METH_OPTIONS) {
      AmSessionFactory::replyOptions(req);
      return;
    }
      
    AmSipDialog::reply_error(req,404,"Not found");
  }

}
