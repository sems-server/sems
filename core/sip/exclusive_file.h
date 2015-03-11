#ifndef _exclusive_file_h_
#define _exclusive_file_h_

#include "AmThread.h"
#include "async_file.h"

#include <string>
using std::string;

class _excl_file_reg;

class exclusive_file
  : public async_file
{
  string   name;
  int      fd;
  
  exclusive_file(const string& name);
  ~exclusive_file();

  int open(bool& is_new);

  // async_file API
  int write_to_file(const void* buf, unsigned int len);
  void on_flushed();

  // called only from _excl_file_reg
  void close() { async_file::close(); }

  friend class _excl_file_reg;

public:
  static int open(const char* filename, exclusive_file*& excl_fp, bool& is_new);
  static void close(const exclusive_file* excl_fp);

  int write(const void *buf, int len);
  int writev(const struct iovec* iov, int iovcnt);
};

#endif
