
#include "MyConfigurableApp.h"

#include "log.h"
#include "AmConfigReader.h"
#include "AmUtils.h"

#define MOD_NAME "myconfigurableapp"

EXPORT_SESSION_FACTORY(MyConfigurableAppFactory,MOD_NAME);

string MyConfigurableAppFactory::AnnouncementFile;

MyConfigurableAppFactory::MyConfigurableAppFactory(const string& _app_name)
  : AmSessionFactory(_app_name)
{
}

int MyConfigurableAppFactory::onLoad()
{
    AmConfigReader cfg;
    if(cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME ".conf")))
	return -1;

    AnnouncementFile = cfg.getParameter("announcement_file","/tmp/default.wav");

    if(!file_exists(AnnouncementFile)){
	ERROR("announcement file for configurableApp module does not exist ('%s').\n",
	      AnnouncementFile.c_str());
	return -1;
    }

    return 0;
}

AmSession* MyConfigurableAppFactory::onInvite(const AmSipRequest& req, const string& app_name,
					      const map<string,string>& app_params)
{
    return new MyConfigurableAppDialog();
}

MyConfigurableAppDialog::MyConfigurableAppDialog()
{
}

MyConfigurableAppDialog::~MyConfigurableAppDialog()
{
}

void MyConfigurableAppDialog::onSessionStart()
{
    DBG("MyConfigurableAppDialog::onSessionStart - file is '%s'\n", 
	MyConfigurableAppFactory::AnnouncementFile.c_str());

    AmSession::onSessionStart();
}

void MyConfigurableAppDialog::onBye(const AmSipRequest& req)
{
    DBG("onBye: stopSession\n");
    setStopped();
}

