#ifndef _VOICEBOX_DIALOG_H_
#define _VOICEBOX_DIALOG_H_

#include "AmApi.h"
#include "AmSession.h"
#include "AmAudio.h"
#include "AmPlaylist.h"
#include "AmPromptCollection.h"
#include "PromptOptions.h"

#include <map>
#include <string>
#include <list>
using std::map;
using std::string;
using std::list;

struct Message {
  string name;
  int size;

  int operator<(const Message& b) const 
  { return name < b.name; }

  Message() { }
  Message(string n, int s)
    : name(n), size(s) { }
};

class VoiceboxDialog 
  : public AmSession
{
public:
  enum VoiceboxCallState {
    None,           // starting
    EnteringPin,    // checking mailbox pin
    Prompting,      // playing prompt (not in message yet)
    MsgAction,      // accepting action on message
    PromptTurnover, // prompting to turn over to first message
    Bye             // term dialog
  }; 

private:
  AmPlaylist  play_list;
  // we need only one separator in queue
  auto_ptr<AmPlaylistSeparator> playlist_separator;
  AmPromptCollection* prompts;
  PromptOptions prompt_options;

  VoiceboxCallState state;
  string entered_pin;

  string user;
  string domain;
  string pin;

  void openMailbox();
  void closeMailbox();
  FILE* getCurrentMessage();

  // logic ops
  void doMailboxStart();
  void doListOverview();
  void checkFinalMessage();
  void mergeMsglists();

  // msg ops
  inline bool enqueueCurMessage();
  inline void repeatCurMessage();
  inline void advanceMessage();
  inline void deleteCurMessage();
  inline void saveCurMessage();
  inline void curMsgOP(const char* op);
  inline void enqueueCount(unsigned int cnt);
  inline void enqueueSeparator(int id);
  inline bool isAtEnd();
  inline bool isAtLastMsg();
  inline void gotoFirstSavedMessage();

  list<Message> new_msgs;
  list<Message> saved_msgs;

  // list of the messages that come be in the msg list the next round 
  list<Message> edited_msgs; 

  bool userdir_open;       // have we opened the user dir? 
  bool do_save_cur_msg;    // saving of current message possible? 

  list<Message>::iterator cur_msg;
  bool in_saved_msgs;
  AmAudioFile message; // message file being played

  AmDynInvoke* msg_storage;
public:
  VoiceboxDialog(const string& user,
		 const string& domain,
		 const string& pin, 
		 AmPromptCollection* prompts,
		 PromptOptions prompt_options);
  ~VoiceboxDialog();

  void onSessionStart();
  void onDtmf(int event, int duration);
  void onBye(const AmSipRequest& req);
  void process(AmEvent* ev);
};


#endif


// Local Variables:
// mode:C++ // Stroustrup? 
// End:

