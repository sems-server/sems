#ifndef _ISDNGATEWAYDIALOG_H_
#define _ISDNGATEWAYDIALOG_H_

#include "AmApi.h"
#include "AmSession.h"
#include "ampi/UACAuthAPI.h"

class GWSession : public AmSession,  public CredentialHolder
{
 public:
  GWSession(const string& auth_realm, const string& auth_user, const string& auth_pwd);
  ~GWSession();
  
  inline UACAuthCred* getCredentials();  //auth interface
  AmSipRequest invite_req;

static    GWSession* CallFromOutside(std::string &fromnumber, std::string &tonumber, int backend, AmAudio* device);
  void setOtherLeg(AmAudio *otherleg);
  void onProgress(const AmSipReply& reply);
  
//Parent methods
//virtual  void process(AmEvent* ev);
//virtual AmPayloadProviderInterface* getPayloadProvider();
//virtual void negotiate(const string& sdp_body,
//virtual void onDtmf(int event, int duration);
//virtual void onStart(){}
  void onInvite(const AmSipRequest& req);
  void onCancel(const AmSipRequest& req);
  void onSessionStart(const AmSipRequest& req);
  void onSessionStart(const AmSipReply& reply);
//virtual void onEarlySessionStart(const AmSipReply& reply){}
  void onRinging(const AmSipReply& reply);
  void onBye(const AmSipRequest& req);
//virtual void onSipEvent(AmSipEvent* sip_ev);
  void onSipRequest(const AmSipRequest& req);
  void onSipReply(const AmSipReply& reply, AmSipDialog::Status old_dlg_status);
//virtual void onRtpTimeout();
//virtual void onSendRequest(const string& method, const string& content_type, const string& body, string& hdrs, int flags, unsigned int cseq);
//virtual void onSendReply(const AmSipRequest& req, unsigned int code,const string& reason,const string& content_type, const string& body,string& hdrs,int flags)
void on_stop();        

 private:
  UACAuthCred credentials;
  AmAudio *m_OtherLeg;

};
#endif
