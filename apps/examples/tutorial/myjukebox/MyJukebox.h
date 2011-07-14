#ifndef _MYJUKEBOX_H_
#define _MYJUKEBOX_H_

#include "AmSession.h"
#include "AmPlaylist.h"
#include "AmAudioFile.h"

#include <string>
using std::string;

#include <vector>
using std::vector;

class MyJukeboxFactory: public AmSessionFactory
{
public:
    static string JukeboxDir;

    MyJukeboxFactory(const string& _app_name);

    int onLoad();
    AmSession* onInvite(const AmSipRequest& req, const string& app_name,
			const map<string,string>& app_params);
};

class MyJukeboxDialog : public AmSession
{
    AmPlaylist playlist;

    vector<AmAudioFile*> used_audio_files;

 public:
    MyJukeboxDialog();
    ~MyJukeboxDialog();

    void onSessionStart();
    void onDtmf(int event, int duration);
    void process(AmEvent* ev);
    void onBye(const AmSipRequest& req);
};

#endif

