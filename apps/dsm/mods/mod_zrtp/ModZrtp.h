/*
 * Copyright (C) 2014 Stefan Sayer
 *
 * Parts of the development of this module was kindly sponsored by AMTEL Inc.
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
#ifndef _MOD_ZRTP_H
#define _MOD_ZRTP_H
#include "DSMModule.h"

#define MOD_CLS_NAME ZRTPModule 

DECLARE_MODULE_BEGIN(MOD_CLS_NAME);
DECLARE_MODULE_END;

#ifdef WITH_ZRTP
DEF_ACTION_1P(ZRTPSetEnabledAction);
DEF_ACTION_1P(ZRTPSetAllowclearAction);
DEF_ACTION_1P(ZRTPSetAutosecureAction);
DEF_ACTION_1P(ZRTPSetDisclosebitAction);
DEF_ACTION_2P(ZRTPGetSASAction);
DEF_ACTION_1P(ZRTPGetSessionInfoAction);
DEF_ACTION_2P(ZRTPSetVerifiedAction);
DEF_ACTION_2P(ZRTPSetUnverifiedAction);
DEF_ACTION_1P(ZRTPSetSignalingHash);
DEF_ACTION_1P(ZRTPGetSignalingHash);
#endif // WITH_ZRTP

#endif
