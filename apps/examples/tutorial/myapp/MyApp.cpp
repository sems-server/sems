#include "MyApp.h"
#include "log.h"

#define MOD_NAME "myapp"

EXPORT_SESSION_FACTORY(MyAppFactory,MOD_NAME);

MyAppFactory::MyAppFactory(const string& _app_name)
  : AmSessionFactory(_app_name)
{
}

int MyAppFactory::onLoad()
{
    return 0;
}

AmSession* MyAppFactory::onInvite(const AmSipRequest& req)
{
    return new MyAppDialog();
}

MyAppDialog::MyAppDialog()
{
}

MyAppDialog::~MyAppDialog()
{
}

void MyAppDialog::onSessionStart(const AmSipRequest& req)
{
    DBG("MyAppDialog::onSessionStart: Hello World!\n");
}

void MyAppDialog::onBye(const AmSipRequest& req)
{
    DBG("onBye: stopSession\n");
    setStopped();
}

