
#include "MyJukebox.h"

#include "log.h"
#include "AmConfigReader.h"
#include "AmUtils.h"
#include "AmAudioFile.h"

#define MOD_NAME "myjukebox"

EXPORT_SESSION_FACTORY(MyJukeboxFactory,MOD_NAME);

string MyJukeboxFactory::JukeboxDir;

MyJukeboxFactory::MyJukeboxFactory(const string& _app_name)
  : AmSessionFactory(_app_name)
{
}

int MyJukeboxFactory::onLoad()
{
    AmConfigReader cfg;
    if(cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME ".conf")))
	return -1;

    JukeboxDir = cfg.getParameter("jukebox_dir","/tmp/");
    if( !JukeboxDir.empty() 
	&& JukeboxDir[JukeboxDir.length()-1] != '/' )
	JukeboxDir += "/";

    return 0;
}

AmSession* MyJukeboxFactory::onInvite(const AmSipRequest& req)
{
    return new MyJukeboxDialog();
}

MyJukeboxDialog::MyJukeboxDialog()
  : playlist(this)
{
}

MyJukeboxDialog::~MyJukeboxDialog()
{
  // clean playlist items
  playlist.close(false);
}

void MyJukeboxDialog::onSessionStart(const AmSipRequest& req)
{
    DBG("MyJukeboxDialog::onSessionStart - jukedir is '%s'\n", 
	MyJukeboxFactory::JukeboxDir.c_str());
    
    setInOut(&playlist, &playlist);
    setDtmfDetectionEnabled(true);
}

void MyJukeboxDialog::onDtmf(int event, int duration) {
  DBG("MyJukeboxDialog::onDtmf, got event %d, duration %d.\n", event, duration);

  AmAudioFile* wav_file = new AmAudioFile();
  if(wav_file->open(MyJukeboxFactory::JukeboxDir + int2str(event) + ".wav",AmAudioFile::Read)) {
    ERROR("MyJukeboxDialog::onSessionStart: Cannot open file\n");
    delete wav_file;
    return;
  }
  AmPlaylistItem*  item = new AmPlaylistItem(wav_file, NULL);
  playlist.addToPlaylist(item);
}

void MyJukeboxDialog::process(AmEvent* ev)
{
    DBG("AmSession::process\n");

    AmAudioEvent* audio_ev = dynamic_cast<AmAudioEvent*>(ev);
    if(audio_ev && (audio_ev->event_id == AmAudioEvent::noAudio)){
      DBG("MyJukeboxDialog::process: Playlist is empty!\n");
      return;
    }

    AmSession::process(ev);
}

void MyJukeboxDialog::onBye(const AmSipRequest& req)
{
    DBG("onBye: stopSession\n");
    setStopped();
}

