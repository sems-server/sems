#ifndef _async_file_writer_h_

#include "AmThread.h"
#include "singleton.h"

#include <event2/event.h>

class _async_file_writer
  : public AmThread
{
  struct event_base* evbase;
  struct event*  ev_default;

protected:
  _async_file_writer();
  ~_async_file_writer();

  const char *identify() { return "async_file_writer"; }
  void on_stop();
  void run();
  
public:
  void start();

  event_base* get_evbase() const {
    return evbase;
  }
};

typedef singleton<_async_file_writer> async_file_writer;

#endif
