#ifndef _SystemDSM_h_
#define _SystemDSM_h_

#include "AmThread.h"
#include "AmEventQueue.h"
#include "DSMSession.h"
#include "AmSession.h"
#include "DSMStateEngine.h"

#include <string>

using std::string;


class EventProxySession
: public AmSession
{
  AmEventQueueInterface* e;
 public:
  EventProxySession(AmEventQueueInterface* e)
    : e(e) { assert(e); }

  void postEvent(AmEvent* event) { e->postEvent(event); }
};

class SystemDSM
: public AmThread,
  public AmEventQueue,
  public AmEventHandler,
  public DSMSession 

{

  EventProxySession dummy_session;
  AmSharedVar<bool> stop_requested;

  DSMStateEngine engine;
  string startDiagName;
  bool reload;

  // owned by this instance
  std::set<DSMDisposable*> gc_trash;

 public:

  SystemDSM(const DSMScriptConfig& config,
	    const string& startDiagName,
	    bool reload);
  ~SystemDSM();

  void run();
  void on_stop();

// AmEventHandler interface
  void process(AmEvent* event);

// DSMSession interface
   void playPrompt(const string& name, bool loop = false, bool front = false);
   void playFile(const string& name, bool loop, bool front = false);
   void playSilence(unsigned int length, bool front = false);
   void recordFile(const string& name);
   unsigned int getRecordLength();
   unsigned int getRecordDataSize();
   void stopRecord();
   void setInOutPlaylist();
   void setInputPlaylist();
   void setOutputPlaylist();

   void addToPlaylist(AmPlaylistItem* item, bool front = false);
   void flushPlaylist();
   void setPromptSet(const string& name);
   void addSeparator(const string& name, bool front = false);
   void connectMedia();
   void disconnectMedia();
   void mute();
   void unmute();

  /** B2BUA functions */
   void B2BconnectCallee(const string& remote_party,
				const string& remote_uri,
				bool relayed_invite = false);
   void B2BterminateOtherLeg();

  /** insert request in list of received ones */
   void B2BaddReceivedRequest(const AmSipRequest& req);

  /** set headers of outgoing INVITE */
   void B2BsetHeaders(const string& hdr, bool replaceCRLF);

  /** set headers of outgoing INVITE */
   void B2BclearHeaders();

  /** add a header to the headers of outgoing INVITE */
   void B2BaddHeader(const string& hdr);

  /** transfer ownership of object to this session instance */
   void transferOwnership(DSMDisposable* d);

  /** release ownership of object from this session instance */
   void releaseOwnership(DSMDisposable* d);

};

#endif
