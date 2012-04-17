/*
 * Copyright (C) 2007 iptego GmbH
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For a license to use the sems software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * SEMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "CallGen.h"
#include "AmConferenceStatus.h"
#include "AmUtils.h"
#include "log.h"
#include "AmUAC.h"
#include "AmPlugIn.h"
#include "AmSessionContainer.h"
#include "AmMediaProcessor.h"

#include <stdlib.h>
#include <time.h>

#define APP_NAME "callgen"
#define PLAY_FILE "play_file"

#define CALL_TIMER 1 

#include <vector>
using std::vector;

EXPORT_SESSION_FACTORY(CallGenFactory,APP_NAME);
EXPORT_PLUGIN_CLASS_FACTORY(CallGenFactory,APP_NAME);

string      CallGenFactory::DigitsDir;
AmFileCache CallGenFactory::play_file;
string      CallGenFactory::from_host;

CallGenFactory::CallGenFactory(const string& _app_name)
  : AmSessionFactory(_app_name),
    AmDynInvokeFactory(_app_name),
    configured(false), target_args(NULL), scheduled(0)
{
  if (NULL == _instance) {
    _instance = this;
  }
}

CallGenFactory* CallGenFactory::_instance=0;

int CallGenFactory::onLoad()
{
  return getInstance()->load();
}

int CallGenFactory::load() {
  // only execute this once
  if (configured) 
    return 0;
  configured = true;

  AmConfigReader cfg;
  if(cfg.loadFile(AmConfig::ModConfigPath + string(APP_NAME)+ ".conf"))
    return -1;

  // get application specific global parameters
  configureModule(cfg);

  string play_fname = cfg.getParameter(PLAY_FILE, "default.wav");
  if (play_file.load(play_fname)) {
    ERROR("file %s could not be loaded.\n", 
	  play_fname.c_str());
    return -1;
  }

  from_host = cfg.getParameter("from_host", "callgen.example.net");

  // get prompts
  AM_PROMPT_START;
  AM_PROMPT_END(prompts, cfg, APP_NAME);

  DigitsDir = cfg.getParameter("digits_dir");
  if (DigitsDir.length() && DigitsDir[DigitsDir.length()-1]!='/')
    DigitsDir+='/';

  if (!DigitsDir.length()) {
    WARN("No digits_dir specified in configuration.\n");
  }
  for (int i=0;i<10;i++) 
    prompts.setPrompt(int2str(i), DigitsDir+int2str(i)+".wav", APP_NAME);

  prompts.setPrompt("*", DigitsDir+"s.wav", APP_NAME);
  prompts.setPrompt("#", DigitsDir+"p.wav", APP_NAME);

  start();

  return 0;
}

void CallGenFactory::run() {
  while (true) {
    actions_mut.lock();
    vector<AmArg> todo;
    time_t now;
    time(&now);
    multimap<time_t, AmArg>::iterator it = 
      actions.begin();
    while (it != actions.end()) {
      if (it->first > now)
	break;
      todo.push_back(it->second);
      actions.erase(it);
      it = actions.begin(); 
    }      
    actions_mut.unlock();

    for (vector<AmArg>::iterator it=todo.begin(); 
	 it != todo.end(); it++)  {
      createCall(*it);
      if (scheduled>0)
	scheduled--;
    }

    checkTarget();
    sleep(1);
  }
}

void CallGenFactory::checkTarget() {
  if (!target_args)
    return;

  DBG("%zd active calls, %d current target, %d already scheduled\n",
      active_calls.size(), target_args->get(0).asInt(), scheduled);

  int missing_calls = 
    target_args->get(0).asInt() - active_calls.size() - scheduled;

  if (missing_calls > 0) { 
    AmArg*  to_schedule_args = new AmArg(*target_args);
    (*to_schedule_args)[0] = AmArg(missing_calls);
  
    AmArg ret;
    scheduleCalls(*to_schedule_args, ret);
    scheduled += missing_calls;
  }
}

void CallGenFactory::on_stop(){
  ERROR("not stoppable!\n");
}

AmSession* CallGenFactory::onInvite(const AmSipRequest& req, const string& app_name,
				    const map<string,string>& app_params) {
  ERROR("incoming calls not supported!\n");
  return NULL;
}

// outgoing calls 
AmSession* CallGenFactory::onInvite(const AmSipRequest& req, const string& app_name,
				    AmArg& args)
{  
  size_t cnt = 0; 
  cnt++; // int    ncalls           = args.get(cnt++).asInt();
  cnt++; // int    wait_time_base   = args.get(cnt++).asInt();
  cnt++; // int    wait_time_rand   = args.get(cnt++).asInt();
  string ruri_user        = args.get(cnt++).asCStr();
  string ruri_host        = args.get(cnt++).asCStr();
  cnt++; // int    ruri_rand_digits = args.get(cnt++).asInt();
  int    play_rand_digits = args.get(cnt++).asInt();
  int    call_time_base   = args.get(cnt++).asInt();
  int    call_time_rand   = args.get(cnt++).asInt();

  return new CallGenDialog(prompts, play_rand_digits, 
			   call_time_base, call_time_rand); 
}

void CallGenFactory::invoke(const string& method, 
				  const AmArg& args, 
				  AmArg& ret)
{
  

  if (method == "createCalls"){
    args.assertArrayFmt("iiissiiii");
    instance()->createCalls(args, ret);
  } else if (method == "scheduleCalls"){
    args.assertArrayFmt("iiissiiii");
    instance()->scheduleCalls(args, ret);
  } else if (method == "setTarget"){
    args.assertArrayFmt("iiissiiii");
    instance()->setTarget(args, ret);
  } else if (method == "callGenStats"){
    instance()->callGenStats(args, ret);
  } else if(method == "help"){
    ret.push(
	     "callgen - simple call generator\n"
	     " method: createCalls - create calls (online - takes its time to return)\n"
	     " method: scheduleCalls - schedule calls\n"
	     " method: setTarget - set call count target\n"
	     "\n"
	     " parameters for these functions are always: \n"
	     "  int    ncalls           - number of calls to [make, schedule, set target]\n"
	     "  int    wait_time_base   - wait time btw calls, base value\n"
	     "  int    wait_time_rand   - wait time btw calls, random add (total = base + rand)\n"
	     "  string ruri_user        - user part of ruri\n"
	     "  string ruri_host        - host part of ruri\n"
	     "  int    ruri_rand_digits - no of random digits to add to ruri user\n"
	     "  int    play_rand_digits - no of random digits to play at the beginning\n"
	     "  int    call_time_base   - call timer, base value \n"
	     "  int    call_time_rand   - call timer, random add (total = base + rand)\n"
	     "\n"
	     " method: callGenStats - return some statistics\n"
	     );
  } else if(method == "_list"){
    ret.push("createCalls");
    ret.push("scheduleCalls");
    ret.push("setTarget");
    ret.push("callGenStats");
    ret.push("help");
  } else
    throw AmDynInvoke::NotImplemented(method);
}

void CallGenFactory::createCall(const AmArg& args) {
  size_t cnt = 0; 
  cnt++; // int    ncalls           = args.get(cnt++).asInt();
  cnt++; // int    wait_time_base   = args.get(cnt++).asInt();
  cnt++; // int    wait_time_rand   = args.get(cnt++).asInt();
  string ruri_user        = args.get(cnt++).asCStr();
  string ruri_host        = args.get(cnt++).asCStr();
  int    ruri_rand_digits = args.get(cnt++).asInt();
  cnt++; // int    play_rand_digits = args.get(cnt++).asInt();
  cnt++; // int    call_time_base   = args.get(cnt++).asInt();
  cnt++; // int    call_time_rand   = args.get(cnt++).asInt();

  string from = "sip:callgen@"+from_host;
  string call_ruri = "sip:"+ruri_user;

  for (int i=0;i<ruri_rand_digits;i++) 
    call_ruri+=int2str(rand()%10);
  
  call_ruri+="@"+ruri_host;
  
  AmArg* c_args = new AmArg(args);
  
  DBG("placing new call to %s\n", call_ruri.c_str());
  /* string tag = */ AmUAC::dialout("callgen", // user
				APP_NAME,  
				call_ruri,
				"<" + from +  ">", from, 
				call_ruri, 
				string(""), // callid
				string(""), // headers
				c_args);

}

void CallGenFactory::createCalls(const AmArg& args, AmArg& ret) {
  size_t cnt = 0; 
  int    ncalls           = args.get(cnt++).asInt();
  int    wait_time_base   = args.get(cnt++).asInt();
  int    wait_time_rand   = args.get(cnt++).asInt();

  for (int i=0;i<ncalls;i++) {
    createCall(args);

    int wait_nsec = wait_time_base;
    if (wait_time_rand>0)
      wait_nsec+=(rand()%wait_time_rand);

    DBG("sleeping %d seconds\n", wait_nsec);
    if (wait_nsec >0 && (i+1 < ncalls))
      sleep(wait_nsec);
  }

  ret.push(0);
  ret.push("OK");  
}

void CallGenFactory::scheduleCalls(const AmArg& args, AmArg& ret) {

  size_t cnt = 0; 
  int    ncalls           = args.get(cnt++).asInt();
  int    wait_time_base   = args.get(cnt++).asInt();
  int    wait_time_rand   = args.get(cnt++).asInt();
  DBG("scheduling %d calls...\n", ncalls);
  
  time_t now;
  time(&now);
  actions_mut.lock();
  for (int i=0;i<ncalls;i++) {
    actions.insert(std::make_pair(now, args));
    
    int wait_nsec = wait_time_base;
    if (wait_time_rand>0)
      wait_nsec+=(rand()%wait_time_rand);

    now+=wait_nsec;
  }  
  actions_mut.unlock();  
}

void CallGenFactory::setTarget(const AmArg& args, AmArg& ret) {
  AmArg* old_args = target_args;
  target_args = new AmArg(args);

  if (old_args)
    delete old_args;

  DBG("target now set to %d calls\n", 
      target_args->get(0).asInt());
}

void CallGenFactory::callGenStats(const AmArg& args, AmArg& ret) {
  int target = 0;
  if (target_args) 
    target = target_args->get(0).asInt();

  string res = "CallGen statistics: \n " +
    int2str((unsigned int)active_calls.size()) + " active calls\n " +
    int2str(target) + " current target\n " +
    int2str(scheduled) +" scheduled\n ";

  calls_list_mut.lock();
  res += int2str((unsigned int)past_calls.size()) + " total calls\n ";
  calls_list_mut.unlock();
  ret.push(res.c_str());
}

void CallGenFactory::reportCall(string callid,
				CallGenEvent ev, 
				time_t connect_ts,
				time_t disconnect_ts) {

  calls_list_mut.lock();
  // FIXME: not 100% correct: disconnect should be moving to past_calls
  if (ev == CGDestroy) { 
    active_calls.erase(callid);
    CallInfo ci;
    ci.connect_ts = connect_ts;
    ci.disconnect_ts = disconnect_ts;
    ci.status = ev;
    past_calls[callid] = ci;
  } else {
    active_calls[callid].connect_ts = connect_ts;
    active_calls[callid].disconnect_ts = disconnect_ts;
    active_calls[callid].status = ev;
  }
  calls_list_mut.unlock();
}

CallGenDialog::CallGenDialog(AmPromptCollection& prompts, 
		int play_rand_digits, int call_time_base, int call_time_rand)
  : play_list(this), prompts(prompts),
    connect_ts(-1), disconnect_ts(-1), 
    play_rand_digits(play_rand_digits), 
    call_time_base(call_time_base), 
    call_time_rand(call_time_rand),
    play_file(&CallGenFactory::play_file),
    timer_started(false)
{
}

CallGenDialog::~CallGenDialog()
{
  prompts.cleanup((long)this);
  play_list.flush();
  report(CGDestroy);
}

void CallGenDialog::onInvite(const AmSipRequest& r) {
  report(CGCreate);
  AmSession::onInvite(r);
}

void CallGenDialog::report(CallGenEvent what) {
  CallGenFactory::instance()->reportCall(getLocalTag(), 
					 what,
					 connect_ts, 
					 disconnect_ts);
}

void CallGenDialog::setCallTimer() {
  if (timer_started)
    return;
  timer_started = true;

  int call_timer = call_time_base;
  if (call_time_rand>0)
    call_timer+=rand()%call_time_rand;

  if (call_timer > 0) {
    DBG("setting timer %d %d\n", CALL_TIMER, call_timer);
    if (!setTimer(CALL_TIMER, call_timer)) {
      ERROR("internal: setting timer!\n");
      return;
    }
  }

}

void CallGenDialog::onEarlySessionStart() {
  setCallTimer();

  AmSession::onEarlySessionStart();
}

void CallGenDialog::onSessionStart() {
  time(&connect_ts);  

  report(CGConnect);
					    
  // add some random digits
  for (int i=0;i<play_rand_digits;i++) 
    prompts.addToPlaylist(int2str(rand()%10),  (long)this, play_list);
  if (play_rand_digits > 0) 
    prompts.addToPlaylist("#",  (long)this, play_list);

  play_file.loop.set(true);
  play_list.addToPlaylist(new AmPlaylistItem(&play_file, NULL));

//   //   todo (?): set loop...
//   prompts.addToPlaylist(PLAY_FILE,  (long)this, play_list);

  setInOut(&play_list, &play_list);

  setCallTimer();

  AmSession::onSessionStart();
}

void CallGenDialog::process(AmEvent* event)
{
  AmPluginEvent* plugin_event = dynamic_cast<AmPluginEvent*>(event);
  if(plugin_event && plugin_event->name == "timer_timeout" &&
     plugin_event->data.get(0).asInt() == CALL_TIMER) {
    time(&disconnect_ts);
    report(CGDisconnect);

    play_list.flush();
    setInOut(NULL,NULL);
    setStopped();
    dlg.bye();
  }
  else
    AmSession::process(event);
  
}

void CallGenDialog::onBye(const AmSipRequest& req) {
  time(&disconnect_ts);
  report(CGDisconnect);

  play_list.flush();
  setInOut(NULL,NULL);
  setStopped();
}

void CallGenDialog::onSipReply(const AmSipReply& reply, AmSipDialog::Status old_dlg_status) {
  AmSession::onSipReply(reply, old_dlg_status);
  if ((old_dlg_status < AmSipDialog::Connected) &&
      dlg.getStatus() == AmSipDialog::Disconnected) {
    DBG("SIP dialog status change: < Connected -> Disconnected, stopping call\n");
    setStopped();
  }
}
