#include "SBCEventLog.h"
#include "AmAppTimer.h"

#include "AmArg.h"
#include "ampi/MonitoringAPI.h"
#include "AmSessionContainer.h" // monitoring ptr

struct MonitoringEventLogHandler
  : public SBCEventLogHandler
{
  void logEvent(long int timestamp, const string& id, 
		const string& type, const AmArg& event) {

    if(NULL != MONITORING_GLOBAL_INTERFACE) {
      AmArg di_args,ret;
      di_args.push(id);
      di_args.push("ts");
      di_args.push(timestamp);
      di_args.push("type");
      di_args.push(type);
      di_args.push("attrs");
      di_args.push(event);

      MONITORING_GLOBAL_INTERFACE->
	invoke("log", di_args, ret);
    }
  }
};

void _SBCEventLog::useMonitoringLog()
{
  if(NULL != MONITORING_GLOBAL_INTERFACE) {
    setEventLogHandler(new MonitoringEventLogHandler());
    INFO("SBC event log will use the monitoring module\n");
  }
  else {
    ERROR("SBC event log cannot use the monitoring module"
	  " as it is not loaded\n");
  }
}

void _SBCEventLog::setEventLogHandler(SBCEventLogHandler* lh)
{
  log_handler.reset(lh);
}

void _SBCEventLog::logEvent(const string& id, const string& type,
			    const AmArg& event)
{
  if(log_handler.get()) {
    log_handler->logEvent(AmAppTimer::instance()->unix_clock.get(),
			  id, type, event);
  }
}
