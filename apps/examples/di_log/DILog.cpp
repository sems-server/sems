#include "AmPlugIn.h"
#include "log.h"
#include "DILog.h"


#include <iostream>
#include <fstream>
#include <sstream>

#include <stdarg.h>

using namespace std;

#define MOD_NAME "di_log"
#include "log.h"

EXPORT_LOG_FACILITY_FACTORY(DILog, MOD_NAME);
EXPORT_PLUGIN_CLASS_FACTORY(DILog, MOD_NAME);

char DILog::ring_buf[MAX_LINES][MAX_LINE_LEN] = {{0}};
int DILog::pos = 0;

DILog::DILog(const string& name) : AmLoggingFacility(name), AmDynInvokeFactory(name) {
}

DILog* DILog::_instance=0;

DILog* DILog::instance() {
  if(_instance == NULL){
    _instance = new DILog(MOD_NAME);
  }
  return _instance;
}

int DILog::onLoad() {
  DBG("DILog logging ring-buffer loaded.\n");
  return 0;
}

DILog::~DILog() { }

void DILog::invoke(const string& method, const AmArg& args, AmArg& ret) {
  if(method == "dumplog") {
    ret.push(dumpLog().c_str());
  } else if(method == "dumplogtodisk") {
    dumpLog(args.get(0).asCStr());
    ret.push("dumped to disk.\n");
  } else if(method == "help") {
    ret.push("dumplog\n"
	     "dumplogtodisk <path>\n"
	     );
  } else throw AmDynInvoke::NotImplemented(method);
}

void DILog::dumpLog(const char* path) {
  fstream fs(path, ios::out);
  int start = (pos + 1) % MAX_LINES;
  for(int i=0; i<MAX_LINES; i++) {
    fs << ring_buf[(i+start)%MAX_LINES];
  }
}

string DILog::dumpLog() {
  stringstream log;

  int start = (pos + 1) % MAX_LINES;
  for(int i=0; i<MAX_LINES; i++) {
    log << ring_buf[(i+start)%MAX_LINES];
  }
  log << endl;
  return log.str();
}

void DILog::log(int level, pid_t pid, pthread_t tid, 
		const char* func, const char* file, int line, char* msg) {
  strncpy(ring_buf[pos], msg, sizeof(ring_buf[0]));
  pos = (pos + 1) % MAX_LINES;
}

// todo: new() array on load, provide DI for resizing
