/*
 * Copyright (C) 2012 Frafos GmbH
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

#include "SubscriptionDialog.h"
#include "AmSession.h"
#include "AmB2BSession.h"
#include "AmEventDispatcher.h"
#include "AmEventQueueProcessor.h"
#include "SBC.h"

/**
 * SubscriptionDialog
 */

SubscriptionDialog::SubscriptionDialog(SBCCallProfile &profile,
				       vector<AmDynInvoke*> &cc_modules,
				       AmSipSubscription* subscription,
				       atomic_ref_cnt* parent_obj)
  : SimpleRelayDialog(profile, cc_modules, parent_obj),
    subs(subscription)
{
  if(!subs)
    subs = new AmSipSubscription(this,this);
}

SubscriptionDialog::SubscriptionDialog(AmSipSubscription* subscription,
				       atomic_ref_cnt* parent_obj)
  : SimpleRelayDialog(parent_obj),
    subs(subscription)
{
  if(!subs)
    subs = new AmSipSubscription(this,this);
}

SubscriptionDialog::~SubscriptionDialog()
{
  DBG("~SubscriptionDialog: local_tag = %s\n",local_tag.c_str());
  if(subs) delete subs;
}

void SubscriptionDialog::process(AmEvent* ev)
{
  if(ev->event_id == E_SIP_SUBSCRIPTION) {
    SingleSubTimeoutEvent* to_ev = dynamic_cast<SingleSubTimeoutEvent*>(ev);
    if(to_ev) {
      subs->onTimeout(to_ev->timer_id,to_ev->sub);
      return;
    }
  }

  SimpleRelayDialog::process(ev);
}

bool SubscriptionDialog::terminated()
{
  return !(getUsages() > 0);
}

bool SubscriptionDialog::getMappedReferID(unsigned int refer_id, 
					  unsigned int& mapped_id) const
{
  map<unsigned int, unsigned int>::const_iterator id_it =
    refer_id_map.find(refer_id);
  if(id_it != refer_id_map.end()) {
    mapped_id = id_it->second;
    return true;
  }

  return false;
}

void SubscriptionDialog::insertMappedReferID(unsigned int refer_id,
					     unsigned int mapped_id)
{
  refer_id_map[refer_id] = mapped_id;
}

void SubscriptionDialog::onSipRequest(const AmSipRequest& req)
{
  if(!subs->onRequestIn(req))
    return;

  if(req.method == SIP_METH_NOTIFY) {

    string event = getHeader(req.hdrs,SIP_HDR_EVENT,true);
    string id = get_header_param(event,"id");
    event = strip_header_params(event);

    if(event == "refer" && !id.empty()) {

      int id_int=0;
      if(str2int(id,id_int)) {

	unsigned int mapped_id=0;
	if(getMappedReferID(id_int,mapped_id)) {

	  AmSipRequest n_req(req);
	  removeHeader(n_req.hdrs,SIP_HDR_EVENT);
	  n_req.hdrs += SIP_HDR_COLSP(SIP_HDR_EVENT) "refer;id=" 
	    + int2str(mapped_id) + CRLF;

	  SimpleRelayDialog::onSipRequest(n_req);
	  return;
	}
      }
    }
  }

  SimpleRelayDialog::onSipRequest(req);
}

void SubscriptionDialog::onSipReply(const AmSipRequest& req,
				    const AmSipReply& reply, 
				    AmBasicSipDialog::Status old_dlg_status)
{
  if(!subs->onReplyIn(req,reply))
    return;

  //grab cseq-mqpping in case of REFER
  if((reply.code >= 200) && (reply.code < 300) &&
     (reply.cseq_method == SIP_METH_REFER)) {

    if(subs->subscriptionExists(SingleSubscription::Subscriber,
				"refer",int2str(reply.cseq))) {
      // remember mapping for refer event package event-id
      RelayMap::iterator t_req_it = relayed_reqs.find(reply.cseq);
      if(t_req_it != relayed_reqs.end())
	insertMappedReferID(reply.cseq,t_req_it->second);
    }
  }

  SimpleRelayDialog::onSipReply(req,reply,old_dlg_status);
}

void SubscriptionDialog::onRequestSent(const AmSipRequest& req)
{
  subs->onRequestSent(req);
  SimpleRelayDialog::onRequestSent(req);
}

void SubscriptionDialog::onReplySent(const AmSipRequest& req,
				     const AmSipReply& reply)
{
  subs->onReplySent(req,reply);
  SimpleRelayDialog::onReplySent(req,reply);
}
