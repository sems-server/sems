#include "async_file_writer.h"

_async_file_writer::_async_file_writer()
{
  evbase = event_base_new();

  // fake event to prevent the event loop from exiting
  ev_default = event_new(evbase,-1,EV_READ|EV_PERSIST,NULL,NULL);
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
