#ifndef _MSG_STORAGE_H
#define _MSG_STORAGE_H

#include "AmApi.h"

class MsgStorage : public AmDynInvokeFactory, 
		   public AmDynInvoke
{

  static MsgStorage* _instance;

  string msg_dir;

  int msg_new(string domain, string user, string msg_name, FILE* data);
  void msg_get(string domain, string user, string msg_name, AmArg& ret);
  int msg_markread(string domain, string user, string msg_name);
  int msg_delete(string domain, string user, string msg_name);

  void userdir_open(string domain, string user, AmArg& ret);
  int userdir_close(string domain, string user);
  void userdir_getcount(string domain, string user, AmArg& ret);

  inline void filecopy(FILE* ifp, FILE* ofp);
 public:
  MsgStorage(const string& name);
  ~MsgStorage();

  AmDynInvoke* getInstance(){ return _instance; }

  int onLoad();
  void invoke(const string& method, const AmArg& args, AmArg& ret);
};

#endif
