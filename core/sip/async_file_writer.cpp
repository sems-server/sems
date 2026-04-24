#include "async_file_writer.h"
#include "log.h"

_async_file_writer::_async_file_writer()
  : evbase(NULL), ev_default(NULL)
{
  evbase = event_base_new();
  if (!evbase) {
    ERROR("event_base_new() failed: async file writer disabled\n");
    return;
  }

  // fake event to prevent the event loop from exiting
  ev_default = event_new(evbase,-1,EV_READ|EV_PERSIST,NULL,NULL);
  event_add(ev_default,NULL);
}

_async_file_writer::~_async_file_writer()
{
  if (ev_default) event_free(ev_default);
  if (evbase)     event_base_free(evbase);
}

void _async_file_writer::start()
{
  if (!evbase) return;
  event_add(ev_default,NULL);
  AmThread::start();
}

void _async_file_writer::on_stop()
{
  if (!evbase) return;
  event_del(ev_default);
  event_base_loopexit(evbase,NULL);
}

void _async_file_writer::run()
{
  if (!evbase) return;
  /* Start the event loop. */
  event_base_dispatch(evbase);
}
