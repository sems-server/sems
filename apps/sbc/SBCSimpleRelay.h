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
#ifndef _SBCSimpleRelay_h_
#define _SBCSimpleRelay_h_

#include "atomic_types.h"
#include "AmBasicSipDialog.h"
#include "AmSipSubscription.h"
#include "AmEventQueue.h"

#include "SBC.h"
#include "ExtendedCCInterface.h"

#include <map>
#include <list>
using std::map;

class SimpleRelayDialog
  : public AmBasicSipDialog,
    public AmBasicSipEventHandler,
    public AmEventQueue,
    public AmEventHandler,
    public atomic_ref_cnt
{
  atomic_ref_cnt*     parent_obj;
  string              other_dlg;

  typedef map<unsigned int,unsigned int> RelayMap;
  RelayMap relayed_reqs;

  // mediation stuff
  vector<FilterEntry>  headerfilter;
  string               append_headers;
  ReplyTranslationMap  reply_translations;
  bool                 transparent_dlg_id;
  bool                 keep_vias;

  bool finished;

  struct CCModuleInfo {
    ExtendedCCInterface* module;
    void *user_data;
  };
  std::list<CCModuleInfo> cc_ext;

  // relay methods
  int relayRequest(const AmSipRequest& req);
  int relayReply(const AmSipReply& reply);

protected:
  // AmEventHandler
  void process(AmEvent* ev);

  // AmEventQueue
  bool processingCycle();

  void initCCModules(SBCCallProfile &profile, vector<AmDynInvoke*> &cc_modules);

  virtual void onB2BRequest(const AmSipRequest& req);
  virtual void onB2BReply(const AmSipReply& reply);

  /** correctly terminate the dialog; MUST be redefined if "terminated" method
   * is redefined */
  virtual void terminate() { finished = true; }

public:
  SimpleRelayDialog(SBCCallProfile &profile, vector<AmDynInvoke*> &cc_modules,
		    atomic_ref_cnt* parent_obj=NULL);
  SimpleRelayDialog(atomic_ref_cnt* parent_obj=NULL);
  ~SimpleRelayDialog();

  void setParent(atomic_ref_cnt* p_obj) {
    if(parent_obj) dec_ref(parent_obj);
    if(p_obj) inc_ref(p_obj);
    parent_obj = p_obj;
  }

  void setOtherDlg(const string& dlg) {
    other_dlg = dlg;
  }

  const string& getOtherDlg() {
    return other_dlg;
  }

  void setKeepVias(bool kv) {
    keep_vias = kv;
  }

  bool getKeepVias() {
    return keep_vias;
  }

  vector<FilterEntry>&  getHeaderFilter() { return headerfilter; }
  const vector<FilterEntry>&  getHeaderFilter() const { return headerfilter; }

  string& getAppendHeaders() { return append_headers; }
  const string& getAppendHeaders() const { return append_headers; }

  ReplyTranslationMap&  getReplyTranslations() { return reply_translations; }
  const ReplyTranslationMap&  getReplyTranslations() const {
    return reply_translations;
  }

  virtual int initUAC(const AmSipRequest& req, const SBCCallProfile& cp);
  virtual int initUAS(const AmSipRequest& req, const SBCCallProfile& cp);

  virtual bool terminated() { return finished; }

  // AmBasicSipEventHandler interface
  void onSipRequest(const AmSipRequest& req);
  void onSipReply(const AmSipRequest& req,
		  const AmSipReply& reply, 
		  AmBasicSipDialog::Status old_dlg_status);
  void onRequestSent(const AmSipRequest& req);
  void onReplySent(const AmSipRequest& req, const AmSipReply& reply);

  void onRemoteDisappeared(const AmSipReply& reply);
  void onLocalTerminate(const AmSipReply& reply);

  // AmEventQueue
  void finalize();
};

class SBCSimpleRelay
{
public:
  static int start(const SimpleRelayCreator::Relay& relay,
		   const AmSipRequest& req,
		   const SBCCallProfile& cp);
};

#endif
