#ifndef _atomic_types_h_
#define _atomic_types_h_

#if defined(__GNUC__)
# if defined(__GNUC_PATCHLEVEL__)
#  define __GNUC_VERSION__ (__GNUC__ * 10000 \
                            + __GNUC_MINOR__ * 100 \
                            + __GNUC_PATCHLEVEL__)
# else
#  define __GNUC_VERSION__ (__GNUC__ * 10000 \
                            + __GNUC_MINOR__ * 100)
# endif
#else
#error Unsupported compiler
#endif

#if __GNUC_VERSION__ < 40101
#error GCC version >= 4.1.1 is required for proper atomic operations
#endif

#include <assert.h>
#include "log.h"

// 32 bit unsigned integer
class atomic_int
{
  volatile unsigned int i;

public:
  atomic_int() : i(0) {}

  void set(unsigned int val) {
    i = val;
  }

  unsigned int get() {
    return i;
  }

  // ++i;
  unsigned int inc() {
    return __sync_add_and_fetch(&i,1);
  }

  // --i;
  unsigned int dec() {
    return __sync_sub_and_fetch(&i,1);
  }

  // return --ll != 0;
  bool dec_and_test() {
    return dec() == 0;
  };
};

// 64 bit unsigned integer
class atomic_int64
{
  volatile unsigned long long ll;
  
public:
  void set(unsigned long long val) {
#ifndef __LP64__ || !__LP64__
    unsigned long long tmp_ll;
    do {
      tmp_ll = ll;
    }
    while(!__sync_bool_compare_and_swap(&ll, tmp_ll, val));
#else
    ll = val;
#endif
  }
  
  unsigned long long get() {
#ifndef __LP64__ || !__LP64__
    unsigned long long tmp_ll;
    do {
      tmp_ll = ll;
    }
    while(!__sync_bool_compare_and_swap(&ll, tmp_ll, tmp_ll));

    return tmp_ll;
#else
    return ll;
#endif
  }

  // returns ++ll;
  unsigned long long inc() {
    return __sync_add_and_fetch(&ll,1);
  }

  // returns --ll;
  unsigned long long dec() {
    return __sync_sub_and_fetch(&ll,1);
  }

  // return --ll == 0;
  bool dec_and_test() {
    return dec() == 0;
  };
};

class atomic_ref_cnt;
void inc_ref(atomic_ref_cnt* rc);
void dec_ref(atomic_ref_cnt* rc);

class atomic_ref_cnt
  : protected atomic_int
{
protected:
  atomic_ref_cnt()
    : atomic_int() {}

  virtual ~atomic_ref_cnt() {}

  friend void inc_ref(atomic_ref_cnt* rc);
  friend void dec_ref(atomic_ref_cnt* rc);
};

inline void inc_ref(atomic_ref_cnt* rc)
{
  assert(rc);
  rc->inc();
}

inline void dec_ref(atomic_ref_cnt* rc)
{
  assert(rc);
  if(rc->dec_and_test())
    delete rc;
}


#endif
