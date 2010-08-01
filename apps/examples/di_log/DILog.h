#include "AmApi.h"

#include <string>
#include <stdarg.h>

#define MAX_LINES 1000
#define MAX_LINE_LEN 256

class DILog : public AmLoggingFacility, public AmDynInvoke, public AmDynInvokeFactory
{
 private:
  void dumpLog(const char* path);
  string dumpLog();
  static DILog* _instance;
  static char ring_buf[MAX_LINES][MAX_LINE_LEN];
  static int pos;

 public:
  DILog(const string& name);
  ~DILog();
  // DI factory
  AmDynInvoke* getInstance() { return instance(); }
  // DI API
  static DILog* instance();
  void invoke(const string& method, const AmArg& args, AmArg& ret);

  int onLoad();

  // LF API
  //void log(int level, const char* fmt);
  void log(int level, pid_t pid, pthread_t tid, const char* func, const char* file, int line, char* msg);
};
