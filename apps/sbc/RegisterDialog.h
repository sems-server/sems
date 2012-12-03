#ifndef _RegisterDialog_h_
#define _RegisterDialog_h_

#include "SBCSimpleRelay.h"
#include "AmUriParser.h"

class RegisterDialog
  : public SimpleRelayDialog
{
  // copy of original contact we received
  AmUriParser original_contact;

  // contact we are sending
  AmUriParser uac_contact;

  // AmBasicSipDialog interface
  int onTxReply(const AmSipRequest& req, AmSipReply& reply, int& flags);
  int onTxRequest(AmSipRequest& req, int& flags);

  //void onB2BRequest(const AmSipRequest& req);
  void onB2BReply(const AmSipReply& reply);

public:
  RegisterDialog();
  ~RegisterDialog();

  // SimpleRelayDialog interface
  int initUAC(const AmSipRequest& req, const SBCCallProfile& cp);
  int initUAS(const AmSipRequest& req, const SBCCallProfile& cp);

  // AmBasicSipEventHandler interface
  void onSipRequest(const AmSipRequest& req);
  void onSipReply(const AmSipRequest& req,
		  const AmSipReply& reply, 
		  AmBasicSipDialog::Status old_dlg_status);
};

#endif


