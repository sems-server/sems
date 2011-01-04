/*
 * Copyright (C) 2002-2003 Fhg Fokus
 * Copyright (C) 2006 iptego GmbH
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

#include "AmApi.h"
#include "log.h"
#include "AmSession.h"

AmDynInvoke::AmDynInvoke() {}
AmDynInvoke::~AmDynInvoke() {}

void AmDynInvoke::invoke(const string& method, const AmArg& args, AmArg& ret)
{
  throw NotImplemented(method);
}

AmDynInvokeFactory::AmDynInvokeFactory(const string& name)
  : AmPluginFactory(name) 
{
}

AmSessionFactory::AmSessionFactory(const string& name)
  : AmPluginFactory(name)
{
}

AmSession* AmSessionFactory::onInvite(const AmSipRequest& req, 
				      AmArg& session_params) {
  WARN(" discarding session parameters to new session.\n");
  return onInvite(req);
}

AmSession* AmSessionFactory::onRefer(const AmSipRequest& req)
{
  throw AmSession::Exception(488,"Not accepted here");
}

AmSession* AmSessionFactory::onRefer(const AmSipRequest& req, 
				     AmArg& session_params)
{
  WARN(" discarding session parameters to new session.\n");
  return onRefer(req);
}

int AmSessionFactory::configureModule(AmConfigReader& cfg) {
  return 0;//mod_conf.readFromConfig(cfg);
}

void AmSessionFactory::configureSession(AmSession* sess) {
  //SessionTimer::sess->configureSessionTimer(mod_conf);
}

void AmSessionFactory::onOoDRequest(const AmSipRequest& req)
{
    ERROR("sorry, we don't support beginning a new session with "
	  "a '%s' message\n", req.method.c_str());
    
    AmSipDialog::reply_error(req,501,"Not Implemented");
    return;
}

// void AmSessionFactory::postEvent(AmEvent* ev) {
//   ERROR("unhandled Event in %s module\n", getName().c_str());
//   delete ev;
// }

AmSessionEventHandlerFactory::AmSessionEventHandlerFactory(const string& name)
  : AmPluginFactory(name) 
{
}

bool AmSessionEventHandlerFactory::onInvite(const AmSipRequest& req, 
					    AmArg& session_params,
					    AmConfigReader& cfg) {
  WARN("discarding session parameters for new session.\n");
  return onInvite(req, cfg);
}


AmLoggingFacility::AmLoggingFacility(const string& name) 
  : AmPluginFactory(name) 
{
}
