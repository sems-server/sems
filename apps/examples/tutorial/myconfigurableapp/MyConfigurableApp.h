#ifndef _MYCONFIGURABLEAPP_H_
#define _MYCONFIGURABLEAPP_H_

#include "AmSession.h"

#include <string>
using std::string;

class MyConfigurableAppFactory: public AmSessionFactory
{
public:
    static string AnnouncementFile;

    MyConfigurableAppFactory(const string& _app_name);

    int onLoad();
    AmSession* onInvite(const AmSipRequest& req, const string& app_name,
			const map<string,string>& app_params);
};

class MyConfigurableAppDialog : public AmSession
{
    
 public:
    MyConfigurableAppDialog();
    ~MyConfigurableAppDialog();

    void onSessionStart();
    void onBye(const AmSipRequest& req);
};

#endif

