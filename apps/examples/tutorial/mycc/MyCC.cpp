
#include "MyCC.h"

#include "log.h"
#include "AmConfigReader.h"
#include "AmUtils.h"
#include "AmAudioFile.h"
#include "AmPlugIn.h"
#include "AmMediaProcessor.h"

#include <sys/time.h>
#include <time.h>

#define MOD_NAME "mycc"

#define TIMERID_CREDIT_TIMEOUT 1

EXPORT_SESSION_FACTORY(MyCCFactory,MOD_NAME);

 string MyCCFactory::InitialAnnouncement;
 string MyCCFactory::IncorrectPIN;
 string MyCCFactory::OutOfCredit;
 string MyCCFactory::Dialing;
 string MyCCFactory::DialFailed;
 string MyCCFactory::EnterNumber;
 string MyCCFactory::ConnectSuffix;

MyCCFactory::MyCCFactory(const string& _app_name)
  : AmSessionFactory(_app_name)
{
}

int MyCCFactory::onLoad()
{
  AmConfigReader cfg;
  if(cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME ".conf")))
    return -1;
  
  InitialAnnouncement = cfg.getParameter("initial_announcement", "/tmp/hello.wav");
  IncorrectPIN        = cfg.getParameter("incorrect_pin", "/tmp/incorrect_pin.wav");
  OutOfCredit         = cfg.getParameter("out_of_credit", "/tmp/out_of_credit.wav");
  Dialing             = cfg.getParameter("dialing", "/tmp/dialing.wav");
  DialFailed          = cfg.getParameter("dial_failed", "/tmp/dial_failed.wav");
  EnterNumber         = cfg.getParameter("enter_number", "/tmp/enter_number.wav"); 
  ConnectSuffix       = cfg.getParameter("connect_suffix", "@127.0.0.1"); 

  cc_acc_fact = AmPlugIn::instance()->getFactory4Di("cc_acc");
  if(!cc_acc_fact){
    ERROR("could not load cc_acc accounting, please provide a module\n");
    return -1;
  }

  return 0;
}

AmSession* MyCCFactory::onInvite(const AmSipRequest& req, const string& app_name,
				 const map<string,string>& app_params)
{

    AmDynInvoke* cc_acc = cc_acc_fact->getInstance();
    if(!cc_acc){
	ERROR("could not get a cc acc reference\n");
	throw AmSession::Exception(500,"could not get a cc acc reference");
    }

    return new MyCCDialog(cc_acc);
}


MyCCDialog::MyCCDialog(AmDynInvoke* cc_acc)
  : playlist(this), 
    state(CC_Collecting_PIN),
    cc_acc(cc_acc),
    AmB2BCallerSession()
    
{
  set_sip_relay_only(false);
  memset(&acc_start, 0, sizeof(struct timeval));
}

MyCCDialog::~MyCCDialog()
{
}


void MyCCDialog::addToPlaylist(string fname) {
  AmAudioFile* wav_file = new AmAudioFile();
  if(wav_file->open(fname,AmAudioFile::Read)) {
    ERROR("MyCCDialog::addToPlaylist: Cannot open file\n");
    delete wav_file; 
  } else {
    AmPlaylistItem*  item = new AmPlaylistItem(wav_file, NULL);
    playlist.addToPlaylist(item);
  }
}

void MyCCDialog::onSessionStart()
{
    DBG("MyCCDialog::onSessionStart");
    
    AmB2BCallerSession::onSessionStart();

    setInOut(&playlist, &playlist);
    addToPlaylist(MyCCFactory::InitialAnnouncement);

    setDtmfDetectionEnabled(true);

    AmB2BCallerSession::onSessionStart();
}

void MyCCDialog::onDtmf(int event, int duration) {
  DBG("MyCCDialog::onDtmf, got event %d, duration %d.\n", event, duration);

  switch (state) {
  case CC_Collecting_PIN: {
    // flush the playlist (stop playing) 
    // if a key is entered
    playlist.flush(); 
    
    if(event <10) {
      pin +=int2str(event);
      DBG("pin is now '%s'\n", pin.c_str());
    } else {
      	AmArg di_args,ret;
	di_args.push(pin.c_str());
	cc_acc->invoke("getCredit", di_args, ret);
	credit = ret.get(0).asInt();
      if (credit < 0) {
	addToPlaylist(MyCCFactory::IncorrectPIN);	
	pin = "";
      } else if (credit == 0) {
	addToPlaylist(MyCCFactory::OutOfCredit);
	pin = "";
      } else {
	number = "";
	state = CC_Collecting_Number;
	addToPlaylist(MyCCFactory::EnterNumber);
      }
    } 
  } break;
  case CC_Collecting_Number: {
    // flush the playlist (stop playing) 
    // if a key is entered
    playlist.flush(); 
    if(event <10) {
      number +=int2str(event);
      DBG("number is now '%s'\n", number.c_str());
    } else {
      if (getCalleeStatus() == None) {
	state = CC_Dialing;
	connectCallee(number + " <sip:" + number+ MyCCFactory::ConnectSuffix + ">", 
		      "sip:"+number+MyCCFactory::ConnectSuffix);
	addToPlaylist(MyCCFactory::Dialing);
      }
    }   
  }
    break;
    case CC_Dialing: 
    case CC_Connected: 
  default: break;
  };
}

void MyCCDialog::process(AmEvent* ev)
{
    DBG("MyCCDialog::process\n");

    AmAudioEvent* audio_ev = dynamic_cast<AmAudioEvent*>(ev);
    if(audio_ev && (audio_ev->event_id == AmAudioEvent::noAudio)){
      DBG("MyCCDialog::process: Playlist is empty!\n");
      return;
    }

    AmPluginEvent* plugin_event = dynamic_cast<AmPluginEvent*>(ev);
    if(plugin_event && plugin_event->name == "timer_timeout") {
      int timer_id = plugin_event->data.get(0).asInt();
      if (timer_id == TIMERID_CREDIT_TIMEOUT) {
	DBG("timer timeout: no credit...\n");
	stopAccounting();
	terminateOtherLeg();
	terminateLeg();
	
	ev->processed = true;
	return;
      }
    }

    AmB2BCallerSession::process(ev);
}

bool MyCCDialog::onOtherReply(const AmSipReply& reply) {
  DBG("OnOtherReply \n");
  if (state == CC_Dialing) {
    if (reply.code < 200) {
      DBG("Callee is trying... code %d\n", reply.code);
    } else if(reply.code < 300){
      if (getCalleeStatus()  == Connected) {
	state = CC_Connected;
	startAccounting();
	// clear audio input and output
	setInOut(NULL, NULL);
	// detach from media processor (not in RTP path any more)
	AmMediaProcessor::instance()->removeSession(this);
	// set the call timer
	setTimer(TIMERID_CREDIT_TIMEOUT, credit);
      }
    } else {
      DBG("Callee final error with code %d\n",reply.code);
      addToPlaylist(MyCCFactory::DialFailed);
      number = "";
      state = CC_Collecting_Number;
    }
  }  
  // we dont call
  //  AmB2BCallerSession::onOtherReply(reply);
  // as it tears down the call if callee could
  // not be reached
  return false;
}

void MyCCDialog::onOtherBye(const AmSipRequest& req) {
  DBG("onOtherBye\n");
  stopAccounting();
  AmB2BCallerSession::onOtherBye(req); // will stop the session
}

void MyCCDialog::onBye(const AmSipRequest& req)
{
  DBG("onBye: stopSession\n");
  if (state == CC_Connected) {
    stopAccounting();
  }
  terminateOtherLeg();
  setStopped();
}

void MyCCDialog::startAccounting() {
  gettimeofday(&acc_start,NULL);
  DBG("start accounting at %ld\n", acc_start.tv_sec);
}

void MyCCDialog::stopAccounting() {
  if ((acc_start.tv_sec != 0) || (acc_start.tv_usec != 0)) {
    struct timeval now;
    gettimeofday(&now,NULL);
    DBG("stop accounting at %ld\n", now.tv_sec);
    timersub(&now,&acc_start,&now);
    if (now.tv_usec>500000) now.tv_sec++;
    DBG("Call lasted %ld seconds\n", now.tv_sec);

    AmArg di_args,ret;
    di_args.push(pin.c_str());
    di_args.push((int)now.tv_sec);
    cc_acc->invoke("subtractCredit", di_args, ret);
  }
}
