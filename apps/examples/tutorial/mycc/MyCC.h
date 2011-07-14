#ifndef _MYJUKEBOX_H_
#define _MYJUKEBOX_H_

#include "AmB2BSession.h"
#include "AmPlaylist.h"

#include <string>
using std::string;

class MyCCFactory: public AmSessionFactory
{

  AmDynInvokeFactory* cc_acc_fact;

public:
  static string InitialAnnouncement;
  static string IncorrectPIN;
  static string OutOfCredit;
  static string EnterNumber;
  static string Dialing;
  static string DialFailed;
  static string ConnectSuffix;
  
  MyCCFactory(const string& _app_name);
  
  int onLoad();
  AmSession* onInvite(const AmSipRequest& req, const string& app_name,
		      const map<string,string>& app_params);
};

class MyCCDialog : public AmB2BCallerSession
{
  enum {CC_Collecting_PIN = 0,
	CC_Collecting_Number,
	CC_Dialing,
	CC_Connected
  } CallerState;

  AmPlaylist playlist;

  int state;
  
  string pin;
  string number;
  int credit;
  void addToPlaylist(string fname);

  void startAccounting();
  void stopAccounting();
  struct timeval acc_start;

  AmDynInvoke* cc_acc;

 public:
    MyCCDialog(AmDynInvoke* cc_acc);
    ~MyCCDialog();

    void onSessionStart();
    void onDtmf(int event, int duration);
    void process(AmEvent* ev);
    void onBye(const AmSipRequest& req);
 protected:
    bool onOtherReply(const AmSipReply& reply);
    void onOtherBye(const AmSipRequest& req);
};

#endif

