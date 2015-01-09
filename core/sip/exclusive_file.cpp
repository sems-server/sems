#include "exclusive_file.h"
#include "singleton.h"
#include "log.h"

#include <sys/stat.h> 
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <map>
using std::map;

class _excl_file_reg
{
  struct excl_file_entry
  {
    exclusive_file* excl_fp;
    unsigned int    ref_cnt;

    excl_file_entry()
      : excl_fp(NULL),
        ref_cnt(0)
    {}
  };

  map<string,excl_file_entry> files;
  AmMutex                     files_mut;
  
public:
  exclusive_file* get(const string& name, bool& is_new) {
    AmLock l(files_mut);
    map<string,excl_file_entry>::iterator it = files.find(name);
    if(it != files.end()) {
      excl_file_entry& fe = it->second;
      fe.ref_cnt++;
      is_new = false;
      return fe.excl_fp;
    }
    else {
      exclusive_file* fp = new exclusive_file(name);
      if(fp->open() < 0) {
        ERROR("could not open '%s': %s",name.c_str(),strerror(errno));
        delete fp;
        is_new = true;
        return NULL;
      }

      files[name].excl_fp = fp;
      files[name].ref_cnt++;
      is_new = true;
      fp->lock();
      return fp;
    }
  }

  void deref(const string& name) {
    AmLock l(files_mut);
    map<string,excl_file_entry>::iterator it = files.find(name);
    if(it != files.end()) {
      excl_file_entry& fe = it->second;
      if(!(--fe.ref_cnt)) {
        delete fe.excl_fp;
        fe.excl_fp = NULL;
        files.erase(it);
      }
    }
  }
};

typedef singleton<_excl_file_reg> excl_file_reg;

exclusive_file::exclusive_file(const string& name)
  : name(name),fd(-1)
{}

exclusive_file::~exclusive_file()
{
  if(fd >= 0)
    ::close(fd);
}

int exclusive_file::open()
{
  if(fd != -1) {
    ERROR("file already open\n");
    return -1;
  }
  
  fd = ::open(name.c_str(),O_WRONLY | O_CREAT | O_APPEND,
	      S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if(fd < 0) {
    ERROR("could not open file '%s': %s", name.c_str(), strerror(errno));
    return -1;
  }

  return 0;
}

int exclusive_file::open(const char* filename,
                         exclusive_file*& excl_fp, 
                         bool& is_new)
{
  excl_fp = excl_file_reg::instance()->get(filename,is_new);
  if(!excl_fp) return -1;
  return 0;
}

void exclusive_file::close(const exclusive_file* excl_fp)
{
  assert(excl_fp != NULL);
  excl_file_reg::instance()->deref(excl_fp->name);
}

int exclusive_file::write(const void *buf, int len)
{
  int res = ::write(fd, buf, len);
  if (res != len) {
    ERROR("writing to file '%s': %s\n",name.c_str(),strerror(errno));
  }
  return res;
}


