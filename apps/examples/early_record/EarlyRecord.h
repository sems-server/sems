#ifndef _EARLY_RECORD_H_
#define _EARLY_RECORD_H_

#include "AmSession.h"
#include "AmAudioFile.h"
#include "AmUACAuth.h"

#include <string>
using std::string;

class EarlyRecordFactory: public AmSessionFactory
{
public:
    static string RecordDir;

    EarlyRecordFactory(const string& _app_name);

    int onLoad();
    AmSession* onInvite(const AmSipRequest& req, const string& app_name,
			const map<string,string>& app_params);
    AmSession* onInvite(const AmSipRequest& req, const string& app_name,
			AmArg& session_params);

};

class EarlyRecordDialog 
: public AmSession, 
  public CredentialHolder // who invented that stupid name? ;) 
{
    
  string msg_filename;
  AmAudioFile a_msg;
  std::auto_ptr<UACAuthCred> cred;
  
 protected:
  void process(AmEvent* event);
    
 public:
  EarlyRecordDialog(UACAuthCred* credentials);
  ~EarlyRecordDialog();

  void onEarlySessionStart();
  void onSessionStart();
  void onBye(const AmSipRequest& req);
  inline UACAuthCred* getCredentials();
};

#endif

