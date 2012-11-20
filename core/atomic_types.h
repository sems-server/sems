#ifndef _atomic_types_h_
#define _atomic_types_h_

#if	(__GNUC__ > 4) || \
	(__GNUC__ == 4 && __GNUC_MINOR__ >= 1) && \
		( \
		  (defined(__APPLE__) && \
		    ( \
		      defined(__ppc__) || \
		      defined(__i386__) || \
		      defined(__x86_64__) \
		    ) \
		  ) || \
		  (defined(__linux__) && \
		    ( \
		      (defined(__i386__) && (defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4))) || \
		      defined(__ia64__) || \
		      defined(__x86_64__) || \
		      (defined(__powerpc__) && !defined(__powerpc64__)) || \
		      defined(__alpha) \
		     ) \
		  ) \
		  )
#define HAVE_ATOMIC_CAS 1
#else
// #warning Compare and Swap is not supported on this architecture
#define HAVE_ATOMIC_CAS 0
#endif

#include <assert.h>
#include "log.h"

#if !HAVE_ATOMIC_CAS
#include "AmThread.h"
#endif


// 32 bit unsigned integer
class atomic_int
#if !HAVE_ATOMIC_CAS
  : protected AmMutex
#endif
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

#if HAVE_ATOMIC_CAS
  // ++i;
  unsigned int inc() {
    return __sync_add_and_fetch(&i,1);
  }

  // --i;
  unsigned int dec() {
    return __sync_sub_and_fetch(&i,1);
  }
#else // if HAVE_ATOMIC_CAS
  // ++i;
  unsigned int inc() {
    unsigned int res;
    lock();
    res = ++i;
    unlock();
    return res;
  }

  // --i;
  unsigned int dec() {
    unsigned int res;
    lock();
    res = --i;
    unlock();
    return res;
  }
#endif

  // return --ll != 0;
  bool dec_and_test() {
    return dec() == 0;
  };
};

// 64 bit unsigned integer
class atomic_int64
#if !HAVE_ATOMIC_CAS
  : protected AmMutex
#endif
{
  volatile unsigned long long ll;
  
public:
#if HAVE_ATOMIC_CAS
  void set(unsigned long long val) {
#if !defined(__LP64__) || !__LP64__
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
#if !defined(__LP64__) || !__LP64__
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

#else // if HAVE_ATOMIC_CAS

  void set(unsigned long long val) {
#if !defined(__LP64__) || !__LP64__
    this->lock();
    ll = val;
    unlock();
#else
    ll = val;
#endif
  }
  
  unsigned long long get() {
#if !defined(__LP64__) || !__LP64__
    unsigned long long tmp_ll;
    lock();
    tmp_ll = ll;
    unlock();
    return tmp_ll;
#else
    return ll;
#endif
  }

  // returns ++ll;
  unsigned long long inc() {
    unsigned long long res;
    lock();
    res = ++ll;
    unlock();
    return res;
  }

  // returns --ll;
  unsigned long long dec() {
    unsigned long long res;
    lock();
    res = --ll;
    unlock();
    return res;
  }
#endif

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
  virtual void on_destroy() {}

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
  if(rc->dec_and_test()){
    rc->on_destroy();
    delete rc;
  }
}


#endif
