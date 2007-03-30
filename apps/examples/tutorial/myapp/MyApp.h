#ifndef _MYAPP_H_
#define _MYAPP_H_

#include "AmSession.h"

class MyAppFactory: public AmSessionFactory
{
public:
    MyAppFactory(const string& _app_name);

    int onLoad();
    AmSession* onInvite(const AmSipRequest& req);
};

class MyAppDialog : public AmSession
{
    
 public:
    MyAppDialog();
    ~MyAppDialog();

    void onSessionStart(const AmSipRequest& req);
    void onBye(const AmSipRequest& req);
};

#endif

