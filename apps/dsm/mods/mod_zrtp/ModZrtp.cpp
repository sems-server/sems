/*
 * Copyright (C) 2014 Stefan Sayer
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
#include "ModZrtp.h"
#include "log.h"
#include "AmUtils.h"

#include "DSMSession.h"
#include "DSMCoreModule.h"

SC_EXPORT(MOD_CLS_NAME);


MOD_ACTIONEXPORT_BEGIN(MOD_CLS_NAME) {

#ifdef WITH_ZRTP
  DEF_CMD("zrtp.setEnabled", ZRTPSetEnabledAction);
#endif

} MOD_ACTIONEXPORT_END;

MOD_CONDITIONEXPORT_BEGIN(MOD_CLS_NAME) {
#ifdef WITH_ZRTP

  if (cmd == "zrtp.protocolEvent")
    return new TestDSMCondition(params, DSMCondition::ZRTPProtocolEvent);

  if (cmd == "zrtp.securityEvent")
    return new TestDSMCondition(params, DSMCondition::ZRTPSecurityEvent);

#endif
} MOD_CONDITIONEXPORT_END;

#ifdef WITH_ZRTP

EXEC_ACTION_START(ZRTPSetEnabledAction) {
  bool b = resolveVars(arg, sess, sc_sess, event_params) == DSM_TRUE;
  DBG("setting ZRTP to %sabled\n", b?"en":"dis");
  sess->enable_zrtp = b;
} EXEC_ACTION_END;


#endif

// CONST_ACTION_2P(DLGReplyAction, ',', true);
// EXEC_ACTION_START(DLGReplyAction) {

//   if (!sc_sess->last_req.get()) {
//     ERROR("no last request to reply\n");
//     sc_sess->SET_ERRNO(DSM_ERRNO_GENERAL);
//     sc_sess->SET_STRERROR("no last request to reply");
//     return false;
//   }

//   replyRequest(sc_sess, sess, event_params, par1, par2, *sc_sess->last_req.get());
// } EXEC_ACTION_END;
