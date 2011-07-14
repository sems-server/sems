#ifndef _MYANNOUNCEAPP_H_
#define _MYANNOUNCEAPP_H_

#include "AmSession.h"
#include "AmAudioFile.h"

#include <string>
using std::string;

class MyAnnounceAppFactory: public AmSessionFactory
{
public:
    static string AnnouncementFile;

    MyAnnounceAppFactory(const string& _app_name);

    int onLoad();
    AmSession* onInvite(const AmSipRequest& req, const string& app_name,
			const map<string,string>& app_params);
};

class MyAnnounceAppDialog : public AmSession
{
    
    AmAudioFile wav_file;

 public:
    MyAnnounceAppDialog();
    ~MyAnnounceAppDialog();

    void onSessionStart();
    void onBye(const AmSipRequest& req);
};

#endif

