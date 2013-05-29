#include "SBCEventLog.h"
#include "AmAppTimer.h"

#include "AmArg.h"
#include "ampi/MonitoringAPI.h"
#include "AmSessionContainer.h" // monitoring ptr
#include "AmUriParser.h"

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


void _SBCEventLog::logCallStart(const AmSipRequest& req,
				const string& local_tag,
				const string& from_remote_ua,
				const string& to_remote_ua,
				int code, const string& reason)
{
  size_t end;
  AmArg start_event;
  AmUriParser uri_parser;

  start_event["source"] = req.remote_ip;
  start_event["src-port"] = req.remote_port;
  start_event["r-uri"]  = req.r_uri;

  if(uri_parser.parse_contact(req.from,0,end))
    start_event["from"] = uri_parser.uri_str();
  else 
    start_event["from"] = req.from;

  start_event["from-ua"] = from_remote_ua;
  DBG("from-ua: '%s'",from_remote_ua.c_str());

  if(uri_parser.parse_contact(req.to,0,end))
    start_event["to"] = uri_parser.uri_str();
  else 
    start_event["to"] = req.to;

  start_event["to-ua"] = to_remote_ua;
  DBG("to-ua: '%s'",to_remote_ua.c_str());

  start_event["call-id"]  = req.callid;
  start_event["res-code"] = code;
  start_event["reason"]   = reason;

  logEvent(local_tag,
	   code >= 200 && code < 300 ? "call-start" : "call-attempt",
	   start_event);
}

void _SBCEventLog::logCallStart(const AmBasicSipDialog* dlg, int code, 
				const string& reason)
{
  size_t end;
  AmArg start_event;
  AmUriParser uri_parser;

  if(uri_parser.parse_contact(dlg->getLocalParty(),0,end))
    start_event["to"] = uri_parser.uri_str();
  else start_event["to"] = dlg->getLocalParty();

  start_event["from-ua"] = dlg->getRemoteUA();
  
  if(uri_parser.parse_contact(dlg->getRemoteParty(),0,end))
    start_event["from"] = uri_parser.uri_str();
  else start_event["from"] = dlg->getRemoteParty();

  start_event["r-uri"]    = dlg->getLocalUri();
  start_event["call-id"]  = dlg->getCallid();
  start_event["res-code"] = (int)code;
  start_event["reason"]   = reason;
  DBG("from-ua: '%s'",dlg->getRemoteUA().c_str());

  logEvent(dlg->getLocalTag(),
	   code >= 200 && code < 300 ? "call-start" : "call-attempt",
	   start_event);
}

void _SBCEventLog::logCallEnd(const AmSipRequest& req,
			      const string& local_tag,
			      const string& reason,
			      struct timeval* tv)
{
  AmArg end_event;

  end_event["call-id"]  = req.callid;
  end_event["reason"]   = reason;
  end_event["source"]   = req.remote_ip;
  end_event["src-port"] = req.remote_port;
  end_event["r-uri"]    = req.r_uri;
  
  size_t end;
  AmUriParser uri_parser;
  if(uri_parser.parse_contact(req.from,0,end))
    end_event["from"] = uri_parser.uri_str();
  else
    end_event["from"] = req.from;

  if(uri_parser.parse_contact(req.to,0,end))
    end_event["to"] = uri_parser.uri_str();
  else
    end_event["to"] = req.to;

  if(tv && tv->tv_sec) {
    struct timeval call_len;
    gettimeofday(&call_len,NULL);
    timersub(&call_len,tv,&call_len);
    double dlen = call_len.tv_sec;
    dlen += (double)call_len.tv_usec / (double)1000000.0;
    end_event["duration"] = dlen;
  }

  logEvent(local_tag,"call-end",end_event);
}

void _SBCEventLog::logCallEnd(const AmBasicSipDialog* dlg,
			      const string& reason,
			      struct timeval* tv)
{
  AmArg end_event;

  end_event["call-id"] = dlg->getCallid();
  end_event["reason"]  = reason;
  end_event["r-uri"]   = dlg->getLocalUri();
  
  size_t end;
  AmUriParser uri_parser;

  if(uri_parser.parse_contact(dlg->getLocalParty(),0,end))
    end_event["from"] = uri_parser.uri_str();
  else
    end_event["from"] = dlg->getLocalParty();

  if(uri_parser.parse_contact(dlg->getRemoteParty(),0,end))
    end_event["from"] = uri_parser.uri_str();
  else
    end_event["from"] = dlg->getRemoteParty();

  if(tv && tv->tv_sec) {
    struct timeval call_len;
    gettimeofday(&call_len,NULL);
    timersub(&call_len,tv,&call_len);
    double dlen = call_len.tv_sec;
    dlen += (double)call_len.tv_usec / (double)1000000.0;
    end_event["duration"] = dlen;
  }

  logEvent(dlg->getLocalTag(),"call-end",end_event);
}
