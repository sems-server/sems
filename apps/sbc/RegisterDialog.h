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
  void onSendReply(const AmSipRequest& req, unsigned int  code,
		   const string& reason, const string& content_type,
		   const string& body, string& hdrs, int& flags);
  void onSendRequest(const string& method, const string& content_type,
		     const string& body, string& hdrs, int& flags,
		     unsigned int cseq);

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
		  int old_dlg_status);
};

#endif


