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

#ifndef _CCDSM_h_
#define _CCDSM_h_

#include "AmArg.h"
#include "AmApi.h"
#include "AmPlugIn.h"
#include "SBCCallLeg.h"
#include "SBCDSMInstance.h"
#include "ExtendedCCInterface.h"

#define TRACE DBG

class CCDSMModule: public AmObject /* passing through DI */, public AmDynInvoke, public ExtendedCCInterface
{
  private:

    static CCDSMModule* _instance;

    SBCDSMInstance* getDSMInstance(SBCCallProfile &profile);
    void deleteDSMInstance(SBCCallProfile &profile);
    void resetDSMInstance(SBCCallProfile &profile);

  public:

    CCDSMModule() { }
    virtual ~CCDSMModule() { }
    static CCDSMModule* instance();

    virtual void invoke(const string& method, const AmArg& args, AmArg& ret);
    virtual int onLoad() { TRACE(MOD_NAME " call control module loaded.\n"); return 0; }
    virtual void onUnload() { TRACE(MOD_NAME " unloading...\n"); }

    // CC interface
    bool init(SBCCallLeg *call, const map<string, string> &values);
    CCChainProcessing onInitialInvite(SBCCallLeg *call, InitialInviteHandlerParams &params);
    CCChainProcessing onBLegRefused(SBCCallLeg *call, const AmSipReply& reply);
    void onDestroyLeg(SBCCallLeg *call);
    void onStateChange(SBCCallLeg *call, const CallLeg::StatusChangeCause &cause);
    CCChainProcessing onInDialogRequest(SBCCallLeg *call, const AmSipRequest &req);
    CCChainProcessing onInDialogReply(SBCCallLeg *call, const AmSipReply &reply);
    CCChainProcessing onEvent(SBCCallLeg *call, AmEvent *e);
    CCChainProcessing onDtmf(SBCCallLeg *call, int event, int duration);
    CCChainProcessing putOnHold(SBCCallLeg *call);
    CCChainProcessing resumeHeld(SBCCallLeg *call, bool send_reinvite);
    CCChainProcessing createHoldRequest(SBCCallLeg *call, AmSdp &sdp);
    CCChainProcessing handleHoldReply(SBCCallLeg *call, bool succeeded);
    int relayEvent(SBCCallLeg *call, AmEvent *e);

    // simple relay
    bool init(SBCCallProfile &profile, SimpleRelayDialog *relay, void *&user_data);
    void initUAC(const AmSipRequest &req, void *user_data);
    void initUAS(const AmSipRequest &req, void *user_data);
    void finalize(void *user_data);
    void onSipRequest(const AmSipRequest& req, void *user_data);
    void onSipReply(const AmSipRequest& req,
		    const AmSipReply& reply,
		    AmBasicSipDialog::Status old_dlg_status,
		    void *user_data);
    void onB2BRequest(const AmSipRequest& req, void *user_data);
    void onB2BReply(const AmSipReply& reply, void *user_data);
};

class CCDSMFactory : public AmDynInvokeFactory
{
  public:
    CCDSMFactory(const string& name): AmDynInvokeFactory(name) {}

    virtual AmDynInvoke* getInstance() { return CCDSMModule::instance(); }
    virtual int onLoad() { return CCDSMModule::instance()->onLoad(); }
    virtual void onUnload() { CCDSMModule::instance()->onUnload(); }
};


#endif
