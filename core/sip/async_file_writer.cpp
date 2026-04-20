#include "async_file_writer.h"

#include <string>
using std::string;

_async_file_writer::_async_file_writer()
{
  evbase = event_base_new();
  /* Without the bail-out below, a failed event_base_new() (returns NULL on
     OOM / missing backend) would be passed to event_new(NULL,...), which
     dereferences the base inside libevent and segfaults before the singleton
     is ever used. Fail the construction cleanly instead. */
  if (!evbase)
    throw string("event_base_new() failed in async_file_writer ctor");

  // fake event to prevent the event loop from exiting
  ev_default = event_new(evbase,-1,EV_READ|EV_PERSIST,NULL,NULL);
  if (!ev_default) {
    event_base_free(evbase);
    evbase = NULL;
    throw string("event_new() failed in async_file_writer ctor");
  }
  event_add(ev_default,NULL);
}

_async_file_writer::~_async_file_writer()
{
  event_free(ev_default);
  event_base_free(evbase);
}

void _async_file_writer::start()
{
  event_add(ev_default,NULL);
  AmThread::start();
}

void _async_file_writer::on_stop()
{
  event_del(ev_default);
  event_base_loopexit(evbase,NULL);
}

void _async_file_writer::run()
{
  /* Start the event loop. */
  event_base_dispatch(evbase);
}
