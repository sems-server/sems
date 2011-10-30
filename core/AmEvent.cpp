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

