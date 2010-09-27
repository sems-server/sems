#ifndef _MSG_STORAGE_H
#define _MSG_STORAGE_H

#include "AmApi.h"

#include <map>
using std::map;

class MsgStorage : public AmDynInvokeFactory, 
		   public AmDynInvoke
{

  static MsgStorage* _instance;

  string msg_dir;

  typedef map<AmDynInvoke*,string> Listeners;
  Listeners  listeners;
  AmMutex    listeners_mut;
  
  int msg_new(string domain, string user, string msg_name, FILE* data);
  void msg_get(string domain, string user, string msg_name, AmArg& ret);
  int msg_markread(string domain, string user, string msg_name);
  int msg_delete(string domain, string user, string msg_name);

  void userdir_open(string domain, string user, AmArg& ret);
  int userdir_close(string domain, string user);
  void userdir_getcount(string domain, string user, AmArg& ret);

  void events_subscribe(AmDynInvoke* event_sink, string method);
  void events_unsubscribe(AmDynInvoke* event_sink);

  void event_notify(const string& domain, 
		    const string& user, 
		    const string& event);

  inline void filecopy(FILE* ifp, FILE* ofp);

 public:
  MsgStorage(const string& name);
  ~MsgStorage();

  AmDynInvoke* getInstance(){ return _instance; }

  int onLoad();
  void invoke(const string& method, const AmArg& args, AmArg& ret);
};

#endif
