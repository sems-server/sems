#ifndef _RegisterDialog_h_
#define _RegisterDialog_h_

#include "SBCSimpleRelay.h"
#include "AmUriParser.h"

class RegisterDialog
  : public SimpleRelayDialog
{
  // Contacts as sent
  vector<AmUriParser> uac_contacts;
  bool star_contact;

  bool contact_hiding;

  bool reg_caching;
  map<string,AmUriParser> alias_map;
  string         aor; // From-URI
  string         source_ip;
  unsigned short source_port;
  string         transport;
  unsigned short local_if;
  string         from_ua;

  // Max 'Expire' value returned to 
  // registering UAC.
  unsigned int max_ua_expire;

  // Min 'Expire' value sent 
  // to the registrar
  unsigned int min_reg_expire;

  // AmBasicSipDialog interface
  int onTxReply(const AmSipRequest& req, AmSipReply& reply, int& flags);
  int onTxRequest(AmSipRequest& req, int& flags);

  // Helper methods
  //

  // send 200 reply with cached contacts (uac_contacts)
  int replyFromCache(const AmSipRequest& req);

  // inits AoR
  int initAor(const AmSipRequest& req);

  // uses AoR to build alias-map 
  // for * contact
  void fillAliasMap();

  // answers statelessly on errors and
  // REGISTER throttling
  int fixUacContacts(const AmSipRequest& req);

  // dialog info should be set already,
  // so that getOutboundIf() can be called
  void fixUacContactHosts(const AmSipRequest& req, const SBCCallProfile& cp);

  // remove the transport parameter from a URI
  int removeTransport(AmUriParser& uri);

public:
  RegisterDialog(SBCCallProfile &profile, vector<AmDynInvoke*> &cc_modules);
  ~RegisterDialog();

  // SimpleRelayDialog interface
  int initUAC(const AmSipRequest& req, const SBCCallProfile& cp);

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

  static int parseContacts(const string& contacts, vector<AmUriParser>& cv);
};

#endif


