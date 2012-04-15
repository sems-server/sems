/*
 * Copyright (C) 2012 FRAFOS GmbH
 *
 * Development sponsored by Sipwise GmbH.
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
#include "ModSubscription.h"
#include "log.h"
#include "AmUtils.h"

#include "AmSipSubscriptionContainer.h"

SC_EXPORT(MOD_CLS_NAME);

MOD_ACTIONEXPORT_BEGIN(MOD_CLS_NAME) {

  DEF_CMD("subscription.create", SIPSUBCreateAction);
  DEF_CMD("subscription.refresh", SIPSUBRefreshAction);
  DEF_CMD("subscription.remove", SIPSUBRemoveAction);

} MOD_ACTIONEXPORT_END;

MOD_CONDITIONEXPORT_NONE(MOD_CLS_NAME);

CONST_ACTION_2P(SIPSUBCreateAction, ',', true);
EXEC_ACTION_START(SIPSUBCreateAction) {
  string params_s = resolveVars(par1, sess, sc_sess, event_params);
//  string basedir = resolveVars(par2, sess, sc_sess, event_params);

  AmSipSubscriptionInfo info(sc_sess->var[params_s+".domain"],
			     sc_sess->var[params_s+".user"],
			     sc_sess->var[params_s+".from_user"],
			     sc_sess->var[params_s+".pwd"],
			     sc_sess->var[params_s+".proxy"],
			     sc_sess->var[params_s+".event"]);
  info.accept = sc_sess->var[params_s+".accept"];
  info.id = sc_sess->var[params_s+".id"];
  unsigned int expires = 0;
  
  if (sc_sess->var.find(params_s+".expires") != sc_sess->var.end()) {
    str2i(sc_sess->var[params_s+".expires"], expires);
  }

  string handle = AmSipSubscriptionContainer::instance()->
    createSubscription(info, sess->getLocalTag(), expires);

  DBG("got handle '%s'\n", handle.c_str());
  sc_sess->var[params_s+".handle"] = handle;
  if (handle.empty()) {
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("could not create subscription\n");
  } else {
    sc_sess->CLR_ERRNO;
  }
} EXEC_ACTION_END;



CONST_ACTION_2P(SIPSUBRefreshAction, ',', true);
EXEC_ACTION_START(SIPSUBRefreshAction) {
  string handle = resolveVars(par1, sess, sc_sess, event_params);
  string expires = resolveVars(par2, sess, sc_sess, event_params);
  unsigned int expires_i = 0;
  if (!expires.empty())
    str2i(expires, expires_i);

  DBG("refreshing subscription with handle '%s'\n", handle.c_str());
  if (!AmSipSubscriptionContainer::instance()->refreshSubscription(handle, expires_i)) {
    sc_sess->SET_ERRNO(DSM_ERRNO_GENERAL);
    sc_sess->SET_STRERROR("could not refresh subscription\n");
  } else {
    sc_sess->CLR_ERRNO;
  }
} EXEC_ACTION_END;

EXEC_ACTION_START(SIPSUBRemoveAction) {
  string handle = resolveVars(arg, sess, sc_sess, event_params);
  DBG("removing subscription with handle '%s'\n", handle.c_str());
  AmSipSubscriptionContainer::instance()->removeSubscription(handle);
} EXEC_ACTION_END;
