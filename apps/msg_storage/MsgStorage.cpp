
#include "AmPlugIn.h"
#include "log.h"

#include "MsgStorageAPI.h"
#include "MsgStorage.h"

#include "AmConfigReader.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <utime.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>

#define MSG_DIR "/var/spool/voicebox/" // default

MsgStorage* MsgStorage::_instance = 0;

EXPORT_PLUGIN_CLASS_FACTORY(MsgStorage, MOD_NAME);

MsgStorage::MsgStorage(const string& name)
  : AmDynInvokeFactory(name),
    listeners()
{ 
      _instance = this; 
}

MsgStorage::~MsgStorage() { }

int MsgStorage::onLoad() {

  msg_dir = MSG_DIR;
  
  AmConfigReader cfg;
  if(cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME ".conf"))) {
    DBG("no configuration could be loaded, assuming defaults.\n");
  } else {
      msg_dir = cfg.getParameter("storage_dir",MSG_DIR);
      DBG("storage_dir set to '%s'.\n", msg_dir.c_str());
  }

  string path = msg_dir;
  int status = mkdir(path.c_str(), 
		     S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  if (status && (errno != EEXIST)) {
    ERROR("creating storage path '%s': %s\n", 
	  path.c_str(),strerror(errno));
    return -1;
  }

  path = msg_dir + "/_test_dir_";
  status = mkdir(path.c_str(), 
		     S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  if (status && (errno != EEXIST)) {
    ERROR("Write permission check failed. Could not create '%s': %s\n", 
	  path.c_str(),strerror(errno));
    return -1;
  }
  rmdir(path.c_str());

  DBG("MsgStorage loaded.\n");
  return 0;
}

void MsgStorage::invoke(const string& method, 
			const AmArg& args, AmArg& ret) {
  if(method == "msg_new"){
    MessageDataFile* f = 
      dynamic_cast<MessageDataFile*>(args.get(3).asObject());
    if (NULL == f) {
      throw(string("message data is not a file ptr."));
    }
    ret.push(msg_new(args.get(0).asCStr(),
		     args.get(1).asCStr(),
		     args.get(2).asCStr(),
		     f->fp));
  } else if(method == "msg_get"){
    msg_get(args.get(0).asCStr(),
	    args.get(1).asCStr(),
	    args.get(2).asCStr(),
	    ret);
  } else if(method == "msg_markread"){
    ret.push(msg_markread(args.get(0).asCStr(),
			  args.get(1).asCStr(),
			  args.get(2).asCStr()));
  } else if(method == "msg_delete"){
    ret.push(msg_delete(args.get(0).asCStr(),
			args.get(1).asCStr(),
			args.get(2).asCStr()));
  } else if(method == "userdir_open"){
    userdir_open(args.get(0).asCStr(),	      
      args.get(1).asCStr(),
      ret);
  } else if(method == "userdir_close"){
    ret.push(userdir_close(args.get(0).asCStr(),
      args.get(1).asCStr()));
  } else if(method == "userdir_getcount"){
    userdir_getcount(args.get(0).asCStr(),
      args.get(1).asCStr(),
      ret);
  } else if(method == "events_subscribe"){
    events_subscribe(args.get(0).asDynInv(),
		     args.get(1).asCStr());
  } else if(method == "events_unsubscribe"){
    events_unsubscribe(args.get(0).asDynInv());
  } else if(method == "_list"){
    ret.push("msg_new");
    ret.push("msg_get");
    ret.push("msg_markread");
    ret.push("msg_delete");
    
    ret.push("userdir_open");
    ret.push("userdir_close");
    ret.push("userdir_getcount");

    ret.push("events_subscribe");
    ret.push("events_unsubscribe");
  }
  else
    throw AmDynInvoke::NotImplemented(method); 
}


int MsgStorage::msg_new(string domain, string user, 
			string msg_name, FILE* data) {

  string path = msg_dir+ "/" + domain + "/" ;
  int status = mkdir(path.c_str(), 
		     S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  if (status && (errno != EEXIST)) {
    ERROR("creating '%s': %s\n", 
	  path.c_str(),strerror(errno));
    return MSG_EUSRNOTFOUND;
  }

  path = msg_dir+ "/" + domain + "/" + user + "/";
  status = mkdir(path.c_str(), 
		     S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  if (status && (errno != EEXIST)) {
    ERROR("creating '%s': %s\n", 
	  path.c_str(),strerror(errno));
    return MSG_EUSRNOTFOUND;
  }

  DBG("creating '%s'\n", (path + msg_name).c_str());

  FILE* fp = fopen((path + msg_name).c_str(), "wb");
  if (!fp) {
    ERROR("creating '%s': %s\n", 
	  (path + msg_name).c_str(),strerror(errno));
    return MSG_ESTORAGE;
  }

  if (data)
    filecopy(data, fp);
  fclose(fp);

  event_notify(domain,user,"msg_new");

  return MSG_OK;
}

void MsgStorage::msg_get(string domain, string user, 
  string msg_name, AmArg& ret) { 
  string fname = msg_dir + "/" + domain + "/" + user + "/"+ msg_name;
  DBG("looking for  '%s'\n", fname.c_str());

  FILE* fp = fopen(fname.c_str(), "r");
  if (!fp) 
    ret.push(MSG_EMSGNOTFOUND);    
  else 
    ret.push(MSG_OK);    

  AmArg af;
  af.setBorrowedPointer(new MessageDataFile(fp));
  ret.push(af);
}

int MsgStorage::msg_markread(string domain, string user, string msg_name) { 
  string path = msg_dir + "/" +  domain + "/" + user + "/" + msg_name;

  struct stat e_stat;
  if (stat(path.c_str(), &e_stat)) {
    ERROR("cannot stat '%s': %s\n", 
	  path.c_str(),strerror(errno));
    return MSG_EMSGNOTFOUND;
  }

  struct utimbuf buf;
  buf.actime = e_stat.st_mtime+1;
  buf.modtime = e_stat.st_mtime;

  if (utime(path.c_str(), &buf)) {
    ERROR("cannot utime '%s': %s\n", 
	  path.c_str(),strerror(errno));
    return MSG_EREADERROR;
  }

  event_notify(domain,user,"msg_markread");

  return MSG_OK;
}

int MsgStorage::msg_delete(string domain, string user, string msg_name) { 
  // TODO: check the directory lock
  string path = msg_dir + "/" + domain + "/" + user + "/" + msg_name;
  if (unlink(path.c_str())) {
      ERROR("cannot unlink '%s': %s\n", 
	    path.c_str(),strerror(errno));
      return MSG_EMSGNOTFOUND;
  }

  event_notify(domain,user,"msg_delete");

  return MSG_OK;
}

void MsgStorage::userdir_open(string domain, string user, AmArg& ret) { 
  // TODO: block the directory from delete (increase lock)
  string path = msg_dir + "/" +  domain + "/" + user + "/";
  DBG("trying to list '%s'\n", path.c_str());
  DIR* dir = opendir(path.c_str());
  if (!dir) {
    ret.push(MSG_EUSRNOTFOUND);
    ret.push(AmArg()); // empty list
    return;
  }

  int err=0;
  struct dirent* entry;
  AmArg msglist;
  msglist.assertArray(0); // make it an array
  while( ((entry = readdir(dir)) != NULL) && (err == 0) ){
    string msgname(entry->d_name);
      if(!msgname.length() ||
	 msgname[0] == '.'){
	continue;
      }
    struct stat e_stat;
    if (stat((path+msgname).c_str(), &e_stat)) {
      ERROR("cannot stat '%s': %s\n", 
	    (path+msgname).c_str(),strerror(errno));
      continue;
    }
    AmArg msg;
    msg.push(msgname.c_str());
    // TODO: change the system here, st_atime/mtime/... 
    // is not really safe for saving read status!

    if (e_stat.st_atime != e_stat.st_mtime) {
      msg.push(0);      
    } else {      
      msg.push(1);      
    }
    msg.push((int)e_stat.st_size);      

    msglist.push(msg);
  }
  closedir(dir);
  // uh, this is really inefficient...
  ret.push(MSG_OK);
  ret.push(msglist);
}

int MsgStorage::userdir_close(string domain, string user) {   
  // TODO: unblock the directory from delete (decrease lock)
  return 0; 
}

void MsgStorage::userdir_getcount(string domain, string user, AmArg& ret) { 
  // TODO: return some useful value
  ret.push(-1);
}

void MsgStorage::events_subscribe(AmDynInvoke* event_sink, string method)
{
  listeners_mut.lock();
  listeners.insert(make_pair(event_sink,method));
  listeners_mut.unlock();
}

void MsgStorage::events_unsubscribe(AmDynInvoke* event_sink)
{
  listeners_mut.lock();
  listeners.erase(event_sink);
  listeners_mut.unlock();
}

void MsgStorage::event_notify(const string& domain, 
			      const string& user, 
			      const string& event)
{
  AmArg args,ret;
  args.push(domain);
  args.push(user);
  args.push(event);

  listeners_mut.lock();

  for(Listeners::iterator it = listeners.begin();
      it != listeners.end(); ++it) {
    try {
      it->first->invoke(it->second, args, ret);
    }
    catch(...){
      DBG("Unexpected exception while notifying event subscribers");
    }
    ret.clear();
  }

  listeners_mut.unlock();
}

// copies ifp to ofp, blockwise
void MsgStorage::filecopy(FILE* ifp, FILE* ofp) {
  size_t nread;
  char buf[1024];
  
  rewind(ifp);
  while (!feof(ifp)) {
    nread = fread(buf, 1, 1024, ifp);
    if (fwrite(buf, 1, nread, ofp) != nread)
      break;
  }
}





