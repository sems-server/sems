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
#include "AmUtils.h"

#include "VoiceboxDialog.h"
#include "Voicebox.h"

#include "../msg_storage/MsgStorageAPI.h" // error codes

#define enqueueFront(msg) \
  prompts->addToPlaylist(msg, (long)this, play_list, true)

#define enqueueBack(msg) \
  prompts->addToPlaylist(msg, (long)this, play_list, false)

// event ids of playlist separator events
#define PLAYLIST_SEPARATOR_MSG_BEGIN 1 // play back of message starts

const char* MsgStrError(int e) {
  switch (e) {
  case MSG_OK: return "MSG_OK"; break;
  case MSG_EMSGEXISTS: return "MSG_EMSGEXISTS"; break;
  case MSG_EUSRNOTFOUND: return "MSG_EUSRNOTFOUND"; break;
  case MSG_EMSGNOTFOUND: return "MSG_EMSGNOTFOUND"; break;
  case MSG_EALREADYCLOSED: return "MSG_EALREADYCLOSED"; break;
  case MSG_EREADERROR: return "MSG_EREADERROR"; break;
  case MSG_ENOSPC: return "MSG_ENOSPC"; break;
  case MSG_ESTORAGE: return "MSG_ESTORAGE"; break;   
  default: return "Unknown Error";
  }
}

VoiceboxDialog::VoiceboxDialog(const string& user,
			       const string& domain,
			       const string& pin, 
			       AmPromptCollection* prompts,
			       PromptOptions prompt_options)
  : play_list(this), prompts(prompts), prompt_options(prompt_options),
    user(user), domain(domain),
    pin(pin),
    userdir_open(false), do_save_cur_msg(false),
    in_saved_msgs(false)
{
  setDtmfDetectionEnabled(true);
  msg_storage = VoiceboxFactory::MessageStorage->getInstance();
  if(!msg_storage){
    ERROR("could not get a message storage reference\n");
    throw AmSession::Exception(500,"could not get a message storage reference");
  }
}

VoiceboxDialog::~VoiceboxDialog()
{
  // empty playlist items
  play_list.flush();
  prompts->cleanup((long)this);
}

void VoiceboxDialog::onSessionStart() { 
  if (pin.empty()) {
    state = Prompting;
    doMailboxStart();
  } else {
    state = EnteringPin;
    enqueueFront("pin_prompt");
  }

  // set the playlist as input and output
  setInOut(&play_list,&play_list);

  AmSession::onSessionStart();
}

void VoiceboxDialog::onBye(const AmSipRequest& req)
{
  closeMailbox();
  setStopped();
}

void VoiceboxDialog::process(AmEvent* ev)
{
  // audio events
  AmAudioEvent* audio_ev = dynamic_cast<AmAudioEvent*>(ev);
  if (audio_ev  && 
      audio_ev->event_id == AmAudioEvent::noAudio) {
    DBG("########## noAudio event #########\n");

    if (Bye == state) {
      closeMailbox();
      dlg->bye();
      setStopped();
    }

    return;
  }

  AmPlaylistSeparatorEvent* pl_ev = dynamic_cast<AmPlaylistSeparatorEvent*>(ev);
  if (pl_ev) {
    DBG("########## Playlist separator ####\n");

    if (Prompting == state) {
      if (pl_ev->event_id == PLAYLIST_SEPARATOR_MSG_BEGIN){
	// mark message as saved  
	saveCurMessage();
	// now we can accept action on the message
	DBG("Changed state to MsgAction.\n");
	state = MsgAction;
      }
    }

    return;
  }

  AmSession::process(ev);
}

void VoiceboxDialog::onDtmf(int event, int duration)
{
  DBG("VoiceboxDialog::onDtmf: event %d duration %d\n", 
      event, duration);
  
  if (EnteringPin == state) {
    play_list.flush();
    // check pin
    if (event<10) {
      entered_pin += int2str(event);
      DBG("added '%s': PIN is now '%s'.\n", 
	  int2str(event).c_str(), entered_pin.c_str());
    }
    if (event==10 || event==11) { // # and * keys
      if (entered_pin.compare(pin)) { // wrong pin
	entered_pin.clear();
	play_list.flush();
	prompts->addToPlaylist("pin_prompt", (long)this, play_list, true);
      }
    }
    if (!entered_pin.compare(pin)) {
      state = Prompting;
      doMailboxStart();
    }
  }

  if (MsgAction == state) {      
    if ((unsigned int)event == VoiceboxFactory::repeat_key) {
      play_list.flush();
      repeatCurMessage();
    } else if ((unsigned int)event == VoiceboxFactory::save_key) {
      state = Prompting;
      play_list.flush();
      enqueueBack("msg_saved");
      saveCurMessage();
      edited_msgs.push_back(*cur_msg);
      advanceMessage();
      checkFinalMessage();
      if (!isAtEnd()) {
	enqueueCurMessage();
      }
    } else if ((unsigned int)event == VoiceboxFactory::delete_key) { 
      state = Prompting;
      play_list.flush();
      enqueueBack("msg_deleted");
      deleteCurMessage(); 
      advanceMessage();
      checkFinalMessage();
      if (!isAtEnd()) {
	enqueueCurMessage();
      }
    } else if ((unsigned int)event == VoiceboxFactory::startover_key) { 
      if (isAtLastMsg()) {
	edited_msgs.push_back(*cur_msg);
	state = Prompting;
	mergeMsglists();
	gotoFirstSavedMessage();
	enqueueCurMessage();
      }
    }
  }

  if (PromptTurnover == state) {      
    if (((unsigned int)event == VoiceboxFactory::startover_key)
	&& (isAtEnd())) {
      state = Prompting;
      mergeMsglists();
      gotoFirstSavedMessage();
      enqueueCurMessage();
    }
  }
 
}

void VoiceboxDialog::openMailbox() {
  cur_msg = new_msgs.begin();

  AmArg di_args,ret;
  di_args.push(domain.c_str()); // domain
  di_args.push(user.c_str());   // user
  msg_storage->invoke("userdir_open",di_args,ret);  
  if (!ret.size() 
      || !isArgInt(ret.get(0))) {
    ERROR("userdir_open for user '%s' domain '%s'"
	  " returned no (valid) result.\n",
	  user.c_str(), domain.c_str()
	  );
    return;
  }
  userdir_open = true;
  int ecode = ret.get(0).asInt();
  if (MSG_EUSRNOTFOUND == ecode) {
    DBG("empty mailbox for user '%s' domain '%s'.\n",
	  user.c_str(), domain.c_str()
	);
    closeMailbox();
    return;    
  }

  if (MSG_OK != ecode) {
    ERROR("userdir_open for user '%s' domain '%s': %s\n",
	  user.c_str(), domain.c_str(),
	  MsgStrError(ret.get(0).asInt()));
    closeMailbox();
    return;
  } 

  if ((ret.size() < 2) ||
      (!isArgArray(ret.get(1)))) {
    ERROR("userdir_open for user '%s' domain '%s'"
	  " returned too few parameters.\n",
	  user.c_str(), domain.c_str()
	  );
    closeMailbox();
    return;
  }

  for (size_t i=0;i<ret.get(1).size();i++) {
    AmArg& elem = ret.get(1).get(i);
    if (!isArgArray(elem) 
	|| elem.size() != 3) {
      ERROR("wrong element in userdir list.\n");
      continue;
    }
    
    string msg_name  = elem.get(0).asCStr();
    int msg_unread = elem.get(1).asInt();
    int size = elem.get(2).asInt();

    if (size) { // TODO: treat empty messages as well!
      if (msg_unread) {
	new_msgs.push_back(Message(msg_name, size));
      } else {
	saved_msgs.push_back(Message(msg_name, size));
      }
    }
  }

  new_msgs.sort();
  new_msgs.reverse();
  saved_msgs.sort();
  saved_msgs.reverse();
  
  DBG("Got %zd new and %zd saved messages for user '%s' domain '%s'\n",
      new_msgs.size(), saved_msgs.size(),
      user.c_str(), domain.c_str());
 
  if (new_msgs.size()) {
    cur_msg = new_msgs.begin();
    in_saved_msgs = false;
  }  else {
    if (saved_msgs.size())
      cur_msg = saved_msgs.begin();    
    in_saved_msgs = true;
  }
}

void VoiceboxDialog::closeMailbox() {
  if (!userdir_open)
    return;
 
  AmArg di_args,ret;
  di_args.push(domain.c_str()); // domain
  di_args.push(user.c_str());   // user
  msg_storage->invoke("userdir_close",di_args,ret);  
  if (ret.size() &&
      isArgInt(ret.get(0)) &&
      ret.get(0).asInt() != MSG_OK
      ) {
    ERROR("userdir_close for user '%s' domain '%s': %s\n",
	  user.c_str(), domain.c_str(),
	  MsgStrError(ret.get(0).asInt()));
  }
  userdir_open = false;
}

FILE* VoiceboxDialog::getCurrentMessage() {
  string msgname = cur_msg->name;

  DBG("trying to get message '%s' for user '%s' domain '%s'\n",
      msgname.c_str(), user.c_str(), domain.c_str());
  AmArg di_args,ret;
  di_args.push(domain.c_str());  // domain
  di_args.push(user.c_str());    // user
  di_args.push(msgname.c_str()); // msg name

  msg_storage->invoke("msg_get",di_args,ret);  
  if (!ret.size()  
      || !isArgInt(ret.get(0))) {
    ERROR("msg_get for user '%s' domain '%s' msg '%s'"
	  " returned no (valid) result.\n",
	  user.c_str(), domain.c_str(),
	  msgname.c_str()
	  );
    return NULL;
  }
  int ecode = ret.get(0).asInt();
  if (MSG_OK != ecode) {
    ERROR("msg_get for user '%s' domain '%s' message '%s': %s",
	  user.c_str(), domain.c_str(),
	  msgname.c_str(),
	  MsgStrError(ret.get(0).asInt()));
    return NULL;
  } 
  
  if ((ret.size() < 2) ||
      (!isArgAObject(ret.get(1)))) {
    ERROR("msg_get for user '%s' domain '%s' message '%s': invalid return value\n",
	  user.c_str(), domain.c_str(),
	  msgname.c_str());
    return NULL;
  }
  MessageDataFile* f = 
    dynamic_cast<MessageDataFile*>(ret.get(1).asObject());
  if (NULL == f)
    return NULL;

  FILE* fp = f->fp;
  delete f;
  return fp;
}

void VoiceboxDialog::doMailboxStart() {
    openMailbox();
    doListOverview();
    if (new_msgs.empty() && saved_msgs.empty()) {
      state = Bye;
    } else {
      enqueueCurMessage();
    }
}

void VoiceboxDialog::doListOverview() {

  if (new_msgs.empty() && saved_msgs.empty()) {
    enqueueBack("no_msg");
    return;
  }

  enqueueFront("you_have");

  if (!new_msgs.empty()) {    
    if (prompt_options.has_digits && 
	(new_msgs.size() == 1)) {
      // one new message
      enqueueBack("new_msg");
    } else {
      // five 
      if (prompt_options.has_digits) 
	enqueueCount(new_msgs.size());
      // new messages
      enqueueBack("new_msgs");
    }
    if (!saved_msgs.empty())
      enqueueBack("and");
  }

  if (!saved_msgs.empty()) {
    if (prompt_options.has_digits && 
	(saved_msgs.size() == 1)) {
      // one saved message
	enqueueBack("saved_msg");
    } else {
      // fifteen
      if (prompt_options.has_digits) 
	enqueueCount(saved_msgs.size());
      // saved messages
      enqueueBack("saved_msgs");
    }
  }
}

bool VoiceboxDialog::enqueueCurMessage() {
  if (((in_saved_msgs) && (cur_msg == saved_msgs.end()))
      ||((!in_saved_msgs) && (cur_msg == new_msgs.end()))) {
      ERROR("check implementation!\n");
      return false;
  }

  FILE*  fp=getCurrentMessage();
  if (NULL == fp)
    return false;

  if (!in_saved_msgs) {
    if (cur_msg == new_msgs.begin()) 
      enqueueBack("first_new_msg");
    else
      enqueueBack("next_new_msg");    
  } else {
    if (cur_msg == saved_msgs.begin()) 
      enqueueBack("first_saved_msg");
    else 
      enqueueBack("next_saved_msg");
  }
  // notifies the dialog that playback of message starts
  enqueueSeparator(PLAYLIST_SEPARATOR_MSG_BEGIN);
  // enqueue msg
  message.fpopen(cur_msg->name, AmAudioFile::Read, fp);
  play_list.addToPlaylist(new AmPlaylistItem(&message, NULL));
  if (!isAtLastMsg())
    enqueueBack("msg_menu");
  else 
    enqueueBack("msg_end_menu");
  //can do save action on cur message?
  do_save_cur_msg = !in_saved_msgs;

  return true;
}

void VoiceboxDialog::repeatCurMessage() {
  play_list.flush();
  message.rewind();
  play_list.addToPlaylist(new AmPlaylistItem(&message, NULL));
  enqueueBack("msg_menu");
}

void VoiceboxDialog::advanceMessage() {
  if (!in_saved_msgs) {
    if (cur_msg != new_msgs.end())
      cur_msg++;
    if (cur_msg == new_msgs.end()) {
      cur_msg = saved_msgs.begin();
      in_saved_msgs = true;
    }
  } else {
    if (cur_msg != saved_msgs.end())
      cur_msg++;
  }
}

void VoiceboxDialog::gotoFirstSavedMessage() {
  cur_msg = saved_msgs.begin();
  in_saved_msgs = true;
}


void VoiceboxDialog::curMsgOP(const char* op) {
  if (!isAtEnd()) {
    string msgname = cur_msg->name;
    AmArg di_args,ret;
    di_args.push(domain.c_str());  // domain
    di_args.push(user.c_str());    // user
    di_args.push(msgname.c_str()); // msg name
    
    msg_storage->invoke(op,di_args,ret);  

    if ((ret.size() < 1)
	|| !isArgInt(ret.get(0))) {
      ERROR("%s returned wrong result type\n", op);
      return;
    }
    
    int errcode = ret.get(0).asInt();
    if (errcode != MSG_OK) {
      ERROR("%s error: %s\n", 
	    op, MsgStrError(errcode));
    }
  }  
}

void VoiceboxDialog::saveCurMessage() {
  if (do_save_cur_msg) 
    curMsgOP("msg_markread");
  do_save_cur_msg = false;
}

void VoiceboxDialog::deleteCurMessage() {
  curMsgOP("msg_delete");
}

bool VoiceboxDialog::isAtLastMsg() {
  if (in_saved_msgs) {
    if (saved_msgs.empty())
      return true;
    return cur_msg->name == saved_msgs.back().name;
    
  } else {
    if (!saved_msgs.empty() || (new_msgs.empty()))
	return false;
    return cur_msg->name == new_msgs.back().name;
  }
}

bool VoiceboxDialog::isAtEnd() {
  bool res = 
  (in_saved_msgs  && (cur_msg == saved_msgs.end()))
    ||(!in_saved_msgs  && (cur_msg == new_msgs.end()));
  return res;
}

void VoiceboxDialog::checkFinalMessage() {
  if (isAtEnd()) {
    if (!edited_msgs.empty()) {
      enqueueBack("no_more_msg");
      state = PromptTurnover;
    } else {
      state = Bye;   
      enqueueBack("no_msg");
    }
  }
}


void VoiceboxDialog::enqueueCount(unsigned int cnt) {
  if (cnt > 99) {
    ERROR("only support up to 99 messages count.\n");
    return;
  }

  if ((cnt <= 20) || (! (cnt%10))) {
    enqueueBack(int2str(cnt));
    return;
  }
  div_t num = div(cnt, 10);
  if (prompt_options.digits_right) {
    // language has single digits after 10s
    enqueueBack(int2str(num.quot * 10));
    enqueueBack("x"+int2str(num.rem));    
  } else {
    // language has single digits before 10s
    enqueueBack("x"+int2str(num.rem));    
    enqueueBack(int2str(num.quot * 10));
  }
}
 
// only one separator may be in playlist! 
void VoiceboxDialog::enqueueSeparator(int id) {
  playlist_separator.reset(new AmPlaylistSeparator(this, id));
  play_list.addToPlaylist(new AmPlaylistItem(playlist_separator.get(), NULL));
}

// copy edited_msgs to saved_msgs
// so that the user can go throuh them again
void VoiceboxDialog::mergeMsglists() {
  saved_msgs.clear();
  saved_msgs = edited_msgs; 
  edited_msgs.clear();
}
