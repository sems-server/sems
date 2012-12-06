#ifndef _RegisterDialog_h_
#define _RegisterDialog_h_

#include "SBCSimpleRelay.h"
#include "AmUriParser.h"

class RegisterDialog
  : public SimpleRelayDialog
{
  // Normalized original contacts
  vector<AmUriParser> orig_contacts;
  bool star_contact;

  // Contacts as sent
  vector<AmUriParser> uac_contacts;

  bool contact_hiding;

  // AmBasicSipDialog interface
  int onTxReply(const AmSipRequest& req, AmSipReply& reply, int& flags);
  int onTxRequest(AmSipRequest& req, int& flags);

  // helper methods
  int parseContacts(const string& contacts, vector<AmUriParser>& cv);

public:
  RegisterDialog();
  ~RegisterDialog();

  // SimpleRelayDialog interface
  int initUAC(const AmSipRequest& req, const SBCCallProfile& cp);
  int initUAS(const AmSipRequest& req, const SBCCallProfile& cp);

  // AmBasicSipEventHandler interface
  void onSipReply(const AmSipRequest& req,
		  const AmSipReply& reply, 
		  AmBasicSipDialog::Status old_dlg_status);

  // Utility static methods
  static string encodeUsername(const AmUriParser& original_contact,
  			       const AmSipRequest& req,
  			       const SBCCallProfile& cp,
  			       ParamReplacerCtx& ctx);

  static bool decodeUsername(const string& encoded_user, AmUriParser& uri);
};

#endif


