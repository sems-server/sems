/*
 * Copyright (C) 2013 Stefan Sayer
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
#ifndef _MOD_SBC_H
#define _MOD_SBC_H
#include "DSMModule.h"
#include "DSMSession.h"

#define MOD_CLS_NAME SCSBCModule

DECLARE_MODULE_BEGIN(MOD_CLS_NAME);
int preload();
DECLARE_MODULE_END;

DEF_ACTION_2P(MODSBCActionProfileSet);
DEF_ACTION_1P(MODSBCActionStopCall);
DEF_ACTION_2P(MODSBCActionDisconnect);
DEF_ACTION_1P(MODSBCActionSendDisconnectEvent);

DEF_ACTION_1P(MODSBCActionPutOnHold);
DEF_ACTION_1P(MODSBCActionResumeHeld);
DEF_ACTION_1P(MODSBCActionGetCallStatus);

DEF_SCCondition(SBCIsALegCondition);
DEF_SCCondition(SBCIsOnHoldCondition);

DEF_SCCondition(SBCIsDisconnectedCondition);
DEF_SCCondition(SBCIsNoReplyCondition);
DEF_SCCondition(SBCIsRingingCondition);
DEF_SCCondition(SBCIsConnectedCondition);
DEF_SCCondition(SBCIsDisconnectingCondition);

DEF_ACTION_2P(MODSBCActionB2BRelayReliable);
DEF_ACTION_2P(MODSBCActionAddCallee);

DEF_ACTION_1P(MODSBCEnableRelayDTMFReceiving);
DEF_ACTION_1P(MODSBCAddToMediaProcessor);
DEF_ACTION_1P(MODSBCRemoveFromMediaProcessor);

DEF_ACTION_2P(MODSBCRtpStreamsSetReceiving);

#endif
