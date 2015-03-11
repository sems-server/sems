#include "fifo_buffer.h"
#include <string.h>

fifo_buffer::fifo_buffer(unsigned int size)
  : size(size), free_space(size)
{
  data = new unsigned char [size];
  data_end = data + size;
  p_head = p_tail = data;
}

fifo_buffer::~fifo_buffer()
{
  delete [] data;
}

int fifo_buffer::write(const void* buf, unsigned int len)
{
  if(len > free_space) {
    // write all or nothing!
    return -1;
  }

  if((p_head >= p_tail) &&
     (len > data_end - p_head)) {

    // split write
    unsigned int buf_end = data_end - p_head;
    memcpy(p_head, buf, buf_end);
    memcpy(data, (unsigned char*)buf + buf_end, len - buf_end);
    p_head = data + len - buf_end;
  }
  else {

    //direct write
    memcpy(p_head,buf,len);
    p_head += len;
  }

  free_space -= len;
  return len;
}

int fifo_buffer::writev(const struct iovec *iov, int iovcnt)
{
  unsigned int len=0;

  for(int i=0; i<iovcnt; i++) {
    len += iov[i].iov_len;
  }

  if(len > free_space) {
    // write all or nothing!
    return -1;
  }

  for(int i=0; i<iovcnt; i++) {
    write(iov[i].iov_base,iov[i].iov_len);
  }

  return len;
}
