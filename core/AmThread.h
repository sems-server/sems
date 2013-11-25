/*
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. This program is released under
 * the GPL with the additional exemption that compiling, linking,
 * and/or using OpenSSL is allowed.
 *
 * For a license to use the SEMS software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * SEMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/** @file AmThread.h */
#ifndef _AmThread_h_
#define _AmThread_h_

#include <pthread.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>

#include <queue>

/**
 * \brief C++ Wrapper class for pthread mutex
 */
class AmMutex
{
  pthread_mutex_t m;

public:
  AmMutex();
  ~AmMutex();
  void lock();
  void unlock();
};

/**
 * \brief  Simple lock class
 */
class AmLock
{
  AmMutex& m;
public:
  AmLock(AmMutex& _m) : m(_m) {
    m.lock();
  }
  ~AmLock(){
    m.unlock();
  }
};

/**
 * \brief Shared variable.
 *
 * Include a variable and its mutex.
 * @warning Don't use safe functions (set,get)
 * within a {lock(); ... unlock();} block. Use 
 * unsafe function instead.
 */
template<class T>
class AmSharedVar
{
  T       t;
  AmMutex m;

public:
  AmSharedVar(const T& _t) : t(_t) {}
  AmSharedVar() {}

  T get() {
    lock();
    T res = unsafe_get();
    unlock();
    return res;
  }

  void set(const T& new_val) {
    lock();
    unsafe_set(new_val);
    unlock();
  }

  void lock() { m.lock(); }
  void unlock() { m.unlock(); }

  const T& unsafe_get() { return t; }
  void unsafe_set(const T& new_val) { t = new_val; }
};

/**
 * \brief C++ Wrapper class for pthread condition
 */
template<class T>
class AmCondition
{
  T               t;
  pthread_mutex_t m;
  pthread_cond_t  cond;

  void init_cond() {
    pthread_mutex_init(&m,NULL);
    pthread_cond_init(&cond,NULL);
  }

public:
  AmCondition() : t() { init_cond(); }
  AmCondition(const T& _t) : t(_t) { init_cond(); }
    
  ~AmCondition()
  {
    pthread_cond_destroy(&cond);
    pthread_mutex_destroy(&m);
  }
    
  /** Change the condition's value. */
  void set(const T& newval)
  {
    pthread_mutex_lock(&m);
    t = newval;
    if(t)
      pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&m);
  }
    
  T get()
  {
    T val;
    pthread_mutex_lock(&m);
    val = t;
    pthread_mutex_unlock(&m);
    return val;
  }
    
  /** Waits for the condition to be true. */
  void wait_for()
  {
    pthread_mutex_lock(&m);
    while(!t){
      pthread_cond_wait(&cond,&m);
    }
    pthread_mutex_unlock(&m);
  }
  
  /** Waits for the condition to be true or a timeout. */
  bool wait_for_to(unsigned long msec)
  {
    struct timeval now;
    struct timespec timeout;
    int retcode = 0;
    bool ret = false;

    gettimeofday(&now, NULL);
    timeout.tv_sec = now.tv_sec + (msec / 1000);
    timeout.tv_nsec = (now.tv_usec + (msec % 1000)*1000)*1000;
    if(timeout.tv_nsec >= 1000000000){
      timeout.tv_sec++;
      timeout.tv_nsec -= 1000000000;
    }

    pthread_mutex_lock(&m);
    while(!t && !retcode){
      retcode = pthread_cond_timedwait(&cond,&m, &timeout);
    }

    if(t) ret = true;
    pthread_mutex_unlock(&m);

    return ret;
  }
};

/**
 * \brief C++ Wrapper class for pthread
 */
class AmThread
{
  pthread_t _td;
  AmMutex   _m_td;

  AmSharedVar<bool> _stopped;

  static void* _start(void*);

protected:
  virtual void run()=0;
  virtual void on_stop()=0;

public:
  unsigned long _pid;
  AmThread();
  virtual ~AmThread() {}

  virtual void onIdle() {}

  /** Start it ! */
  void start();
  /** Stop it ! */
  void stop();
  /** @return true if this thread doesn't run. */
  bool is_stopped() { return _stopped.get(); }
  /** Wait for this thread to finish */
  void join();
  /** kill the thread (if pthread_setcancelstate(PTHREAD_CANCEL_ENABLED) has been set) **/ 
  void cancel();

  int setRealtime();
};

/**
 * \brief Container/garbage collector for threads. 
 * 
 * AmThreadWatcher waits for threads to stop
 * and delete them.
 * It gets started automatically when needed.
 * Once you added a thread to the container,
 * there is no mean to get it out.
 */
class AmThreadWatcher: public AmThread
{
  static AmThreadWatcher* _instance;
  static AmMutex          _inst_mut;

  std::queue<AmThread*> thread_queue;
  AmMutex          q_mut;

  /** the daemon only runs if this is true */
  AmCondition<bool> _run_cond;
    
  AmThreadWatcher();
  void run();
  void on_stop();

public:
  static AmThreadWatcher* instance();
  void add(AmThread*);
};

template<class T>
class AmThreadLocalStorage
{
  pthread_key_t key;
  
  static void __del_tls_obj(void* obj) {
    delete static_cast<T*>(obj);
  }

public:
  AmThreadLocalStorage() {
    pthread_key_create(&key,__del_tls_obj);
  }

  ~AmThreadLocalStorage() {
    pthread_key_delete(key);
  }

  T* get() {
    return static_cast<T*>(pthread_getspecific(key));
  }

  void set(T* p) {
    pthread_setspecific(key,(void*)p);
  }
};

#endif

// Local Variables:
// mode:C++
// End:

