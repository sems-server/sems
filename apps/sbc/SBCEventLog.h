#ifndef _SBCEventLog_h_
#define _SBCEventLog_h_

#include "singleton.h"
#include "AmArg.h"

#include <memory>
#include <string>
#include <map>
using std::auto_ptr;
using std::string;
using std::map;

struct SBCEventLogHandler
{
  virtual void logEvent(long int timestamp, const string& id, 
			const string& type, const AmArg& ev)=0;
};

class _SBCEventLog
{
  auto_ptr<SBCEventLogHandler> log_handler;

protected:
  _SBCEventLog() {}
  ~_SBCEventLog() {}

public:
  void useMonitoringLog();
  void setEventLogHandler(SBCEventLogHandler* lh);
  void logEvent(const string& id, const string& type, const AmArg& event);
};

typedef singleton<_SBCEventLog> SBCEventLog;

#endif
