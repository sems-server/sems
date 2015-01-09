#ifndef _exclusive_file_h_
#define _exclusive_file_h_

#include "AmThread.h"

#include <string>
using std::string;

class _excl_file_reg;

class exclusive_file
  : public AmMutex
{
  string   name;
  int      fd;

  exclusive_file(const string& name);
  ~exclusive_file();

  int open();

  friend class _excl_file_reg;

public:
  static int open(const char* filename, exclusive_file*& excl_fp, bool& is_new);
  static void close(const exclusive_file* excl_fp);

  int write(const void *buf, int len);
};

#endif
