#include "AmEvent.h"

AmEvent::AmEvent(int event_id)
  : event_id(event_id), processed(false)
{
}

AmEvent::AmEvent(const AmEvent& rhs) 
: event_id(rhs.event_id), processed(rhs.processed)
{
}

AmEvent::~AmEvent()
{
}

AmEvent* AmEvent::clone() {
  return new AmEvent(*this);
}

AmPluginEvent::AmPluginEvent(const string& n, const AmArg& d)
  : AmEvent(E_PLUGIN), name(n), data(d) {}

AmPluginEvent::AmPluginEvent(const string& n)
  : AmEvent(E_PLUGIN), name(n), data() {}

AmTimeoutEvent::AmTimeoutEvent(int timer_id)
  : AmPluginEvent(TIMEOUTEVENT_NAME)
{
  data.push(AmArg(timer_id));
}

AmSystemEvent::AmSystemEvent(EvType e)
  : AmEvent(E_SYSTEM), sys_event(e) { }

AmSystemEvent::AmSystemEvent(const AmSystemEvent& rhs)
    : AmEvent(rhs), sys_event(rhs.sys_event) { }

AmEvent* AmSystemEvent::clone() {  return new AmSystemEvent(*this); };

const char* AmSystemEvent::getDescription(EvType t) {
  switch (t) {
  case ServerShutdown: return "ServerShutdown";
  case User1: return "User1";
  case User2: return "User2";
  default: return "Unknown";
  }
}
