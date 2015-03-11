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

#ifndef EXCL_BUFFER_SIZE
#define EXCL_BUFFER_SIZE 1024*1024 /* 1 MB */
#endif 

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
      if(!fe.ref_cnt) {
        ERROR("trying to re-open a file not yet closed");
        return NULL;
      }

      fe.ref_cnt++;
      is_new = false;
      return fe.excl_fp;
    }
    else {
      exclusive_file* fp = new exclusive_file(name);
      if(fp->open(is_new) < 0) {
        ERROR("could not open '%s': %s",name.c_str(),strerror(errno));
        delete fp;
        return NULL;
      }

      files[name].excl_fp = fp;
      files[name].ref_cnt++;

      if(is_new) fp->lock();
      return fp;
    }
  }

  void deref(const string& name) {
    AmLock l(files_mut);
    map<string,excl_file_entry>::iterator it = files.find(name);
    if(it != files.end()) {
      excl_file_entry& fe = it->second;
      if(!(--fe.ref_cnt)) {
        // async delete
        //  - call close()
        //  - wait for notification of close before deleting
        fe.excl_fp->close();
      }
    }
  }

  bool delete_on_flushed(const string& name) {
    AmLock l(files_mut);
    map<string,excl_file_entry>::iterator it = files.find(name);
    if(it != files.end()) {
      excl_file_entry& fe = it->second;
      if(!fe.ref_cnt) {
        delete fe.excl_fp;
        fe.excl_fp = NULL;
        files.erase(it);
        return true;
      }
    }
    return false;
  }
};

typedef singleton<_excl_file_reg> excl_file_reg;

exclusive_file::exclusive_file(const string& name)
  : async_file(EXCL_BUFFER_SIZE),
    name(name),fd(-1)
{}

exclusive_file::~exclusive_file()
{
  if(fd >= 0)
    ::close(fd);

  DBG("just closed %s",name.c_str());
}

//
// TODO: add a close() method that closes the underlying async_file
//

int exclusive_file::open(bool& is_new)
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

  if(lseek(fd,0,SEEK_END) > 0) {
    is_new = false;
  }
  else {
    is_new = true;
  }

  return 0;
}

int exclusive_file::write_to_file(const void* buf, unsigned int len)
{
  int res = ::write(fd, buf, len);

  if (res != (int)len) {
    ERROR("writing to file '%s': %s\n",name.c_str(),strerror(errno));
  }
  //else {
  //DBG("%i bytes written to %s",res,name.c_str());
  //}

  return res;
}

void exclusive_file::on_flushed()
{
  excl_file_reg::instance()->delete_on_flushed(name);
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
  DBG("async writting %i bytes to %s",len,name.c_str());
  return (int)async_file::write(buf,len);
}

int exclusive_file::writev(const struct iovec* iov, int iovcnt)
{
  // int len=0;
  // for(int i=0; i<iovcnt; i++)
  //   len += iov[i].iov_len;    
  //DBG("async writting (iov) %i bytes to %s",len,name.c_str());

  return (int)async_file::writev(iov,iovcnt);
}

