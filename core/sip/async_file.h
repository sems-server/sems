#ifndef _async_file_h_

#include "fifo_buffer.h"

#include <event2/event.h>

class async_file
  : protected fifo_buffer,
    public AmMutex
{
  struct event_base* evbase;
  struct event*    ev_write;

  bool closed;
  bool error;

  /** libevent call-back */
  static void write_cb(int sd, short what, void* ctx);

  /**
   * Triggers real writing into the file itself.
   *
   * If this instance has been closed before, the buffer
   * will be flushed and close_file() will be called.
   */
  void write_cycle();

  unsigned int write_thresh;

protected:
  /**
   * Write to the file itself
   *
   * returns the number of bytes written or -1.
   */
  virtual int write_to_file(const void* buf, unsigned int len)=0;

  virtual void on_flushed()=0;

public:

  async_file(unsigned int buf_len);
  virtual ~async_file();

  enum StatusCode {
    OK=0,
    BufferFull=-1,
    Closed=-2,
    Error=-3
  };

  /**
   * Write into the file buffer
   *
   * return -1 in case the buffer could not be written
   * into the buffer.
   */
  int write(const void* buf, unsigned int len);
  int writev(const struct iovec *iov, int iovcnt);

  /**
   * Mark the file as closed
   */
  void close();

  unsigned int get_buffered_bytes();
};

#endif
