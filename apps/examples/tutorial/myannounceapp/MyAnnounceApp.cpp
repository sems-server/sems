
#include "MyAnnounceApp.h"

#include "log.h"
#include "AmConfigReader.h"
#include "AmUtils.h"

#define MOD_NAME "myannounceapp"

EXPORT_SESSION_FACTORY(MyAnnounceAppFactory,MOD_NAME);

string MyAnnounceAppFactory::AnnouncementFile;

MyAnnounceAppFactory::MyAnnounceAppFactory(const string& _app_name)
  : AmSessionFactory(_app_name)
{
}

int MyAnnounceAppFactory::onLoad()
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

AmSession* MyAnnounceAppFactory::onInvite(const AmSipRequest& req, const string& app_name,
					  const map<string,string>& app_params)
{
    return new MyAnnounceAppDialog();
}

MyAnnounceAppDialog::MyAnnounceAppDialog()
{
}

MyAnnounceAppDialog::~MyAnnounceAppDialog()
{
}

void MyAnnounceAppDialog::onSessionStart()
{
    DBG("MyAnnounceAppDialog::onSessionStart - file is '%s'\n", 
	MyAnnounceAppFactory::AnnouncementFile.c_str());

    if(wav_file.open(MyAnnounceAppFactory::AnnouncementFile,AmAudioFile::Read))
	throw string("MyAnnounceAppDialog::onSessionStart: Cannot open file\n");
    
    setOutput(&wav_file);

    AmSession::onSessionStart();
}

void MyAnnounceAppDialog::onBye(const AmSipRequest& req)
{
    DBG("onBye: stopSession\n");
    setStopped();
}

