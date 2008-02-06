/*
 * $Id: AmThread.cpp 390 2007-07-06 23:57:00Z sayer $
 *
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of sems, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * sems is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "AmThread.h"
#include "log.h"

#include <unistd.h>
#include "errno.h"
#include <string>
using std::string;

AmMutex::AmMutex() 
{
  pthread_mutex_init(&m,NULL);
}

AmMutex::~AmMutex()
{
  pthread_mutex_destroy(&m);
}

void AmMutex::lock() 
{
  pthread_mutex_lock(&m);
}

void AmMutex::unlock() 
{
  pthread_mutex_unlock(&m);
}

AmThread::AmThread()
  : _stopped(true)
{
}

// int thread_nr=0;
// AmMutex thread_nr_mut;

void * AmThread::_start(void * _t)
{
  AmThread* _this = (AmThread*)_t;
  _this->_pid = (pid_t) _this->_td;
  DBG("Thread %lu is starting.\n", (unsigned long int) _this->_pid);
  _this->run();
  _this->_stopped.set(true);
    
  //thread_nr_mut.lock();
  //INFO("threads = %i\n",--thread_nr);
  //thread_nr_mut.unlock();

  DBG("Thread %lu is ending.\n", (unsigned long int) _this->_pid);
  return NULL;
}

void AmThread::start(bool realtime)
{
  // start thread realtime...seems to not improve any thing
  //
  //     if (realtime) {
  // 	pthread_attr_t attributes;	
  // 	pthread_attr_init(&attributes);
  // 	struct sched_param rt_param;
	
  // 	if (pthread_attr_setschedpolicy (&attributes, SCHED_FIFO)) {
  // 	    ERROR ("cannot set FIFO scheduling class for RT thread");
  // 	}
    
  // 	if (pthread_attr_setscope (&attributes, PTHREAD_SCOPE_SYSTEM)) {
  // 	    ERROR ("Cannot set scheduling scope for RT thread");
  // 	}
	
  // 	memset (&rt_param, 0, sizeof (rt_param));
  // 	rt_param.sched_priority = 80;

  // 	if (pthread_attr_setschedparam (&attributes, &rt_param)) {
  // 	    ERROR ("Cannot set scheduling priority for RT thread (%s)", strerror (errno));
  // 	}
  // 	int res;
  // 	_pid = 0;
  // 	res = pthread_create(&_td,&attributes,_start,this);
  // 	if (res != 0) {
  // 	    ERROR("pthread create of RT thread failed with code %s\n", strerror(res));
  // 	    ERROR("Trying to start normal thread...\n");
  // 	    _pid = 0;
  // 	    res = pthread_create(&_td,NULL,_start,this);
  // 	    if (res != 0) {
  // 		ERROR("pthread create failed with code %i\n", res);
  // 	    }	
  // 	}	
  // 	DBG("Thread %ld is just created.\n", (unsigned long int) _pid);
  // 	return;
  //     }

  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr,1024*1024);// 1 MB

  int res;
  _pid = 0;
  res = pthread_create(&_td,&attr,_start,this);
  pthread_attr_destroy(&attr);
  if (res != 0) {
    ERROR("pthread create failed with code %i\n", res);
    throw string("thread could not be started");
  }	
  //     thread_nr_mut.lock();
  //     INFO("threads = %i\n",++thread_nr);
  //     thread_nr_mut.unlock();

  _stopped.set(false);

  //DBG("Thread %lu is just created.\n", (unsigned long int) _pid);
}

void AmThread::stop()
{
  _m_td.lock();

  // gives the thread a chance to clean up
  DBG("Thread %lu (%lu) calling on_stop, give it a chance to clean up.\n", 
      (unsigned long int) _pid, (unsigned long int) _td);

  try { on_stop(); } catch(...) {}

  int res;
  if ((res = pthread_detach(_td)) != 0) {
    if (res == EINVAL) {
      WARN("pthread_detach failed with code EINVAL: thread already in detached state.\n");
    } else if (res == ESRCH) {
      WARN("pthread_detach failed with code ESRCH: thread could not be found.\n");
    } else {
      WARN("pthread_detach failed with code %i\n", res);
    }
  }

  DBG("Thread %lu (%lu) finished detach.\n", (unsigned long int) _pid, (unsigned long int) _td);

  //pthread_cancel(_td);

  _m_td.unlock();
}

void AmThread::cancel() {
  _m_td.lock();

  int res;
  if ((res = pthread_cancel(_td)) != 0) {
    ERROR("pthread_cancel failed with code %i\n", res);
  } else {
    DBG("Thread %lu is canceled.\n", (unsigned long int) _pid);
    _stopped.set(true);
  }

  _m_td.unlock();
}

void AmThread::join()
{
  if(!is_stopped())
    pthread_join(_td,NULL);
}


int AmThread::setRealtime() {
  // set process realtime
  //     int policy;
  //     struct sched_param rt_param;
  //     memset (&rt_param, 0, sizeof (rt_param));
  //     rt_param.sched_priority = 80;
  //     int res = sched_setscheduler(0, SCHED_FIFO, &rt_param);
  //     if (res) {
  // 	ERROR("sched_setscheduler failed. Try to run SEMS as root or suid.\n");
  //     }

  //     policy = sched_getscheduler(0);
    
  //     std::string str_policy = "unknown";
  //     switch(policy) {
  // 	case SCHED_OTHER: str_policy = "SCHED_OTHER"; break;
  // 	case SCHED_RR: str_policy = "SCHED_RR"; break;
  // 	case SCHED_FIFO: str_policy = "SCHED_FIFO"; break;
  //     }
 
  //     DBG("Thread has now policy '%s' - priority 80 (from %d to %d).\n", str_policy.c_str(), 
  // 	sched_get_priority_min(policy), sched_get_priority_max(policy));
  //     return 0;
  return 0;
}


AmThreadWatcher* AmThreadWatcher::_instance=0;
AmMutex AmThreadWatcher::_inst_mut;

AmThreadWatcher::AmThreadWatcher()
  : _run_cond(false)
{
}

AmThreadWatcher* AmThreadWatcher::instance()
{
  _inst_mut.lock();
  if(!_instance){
    _instance = new AmThreadWatcher();
    _instance->start();
  }

  _inst_mut.unlock();
  return _instance;
}

void AmThreadWatcher::add(AmThread* t)
{
  DBG("trying to add thread %lu to thread watcher.\n", (unsigned long int) t->_pid);
  q_mut.lock();
  thread_queue.push(t);
  _run_cond.set(true);
  q_mut.unlock();
  DBG("added thread %lu to thread watcher.\n", (unsigned long int) t->_pid);
}

void AmThreadWatcher::on_stop()
{
}

void AmThreadWatcher::run()
{
  for(;;){

    _run_cond.wait_for();
    // Let some time for to threads 
    // to stop by themselves
    sleep(10);

    q_mut.lock();
    DBG("Thread watcher starting its work\n");

    try {
      queue<AmThread*> n_thread_queue;

      while(!thread_queue.empty()){

	AmThread* cur_thread = thread_queue.front();
	thread_queue.pop();

	q_mut.unlock();
	DBG("thread %lu is to be processed in thread watcher.\n", (unsigned long int) cur_thread->_pid);
	if(cur_thread->is_stopped()){
	  DBG("thread %lu has been destroyed.\n", (unsigned long int) cur_thread->_pid);
	  delete cur_thread;
	}
	else {
	  DBG("thread %lu still running.\n", (unsigned long int) cur_thread->_pid);
	  n_thread_queue.push(cur_thread);
	}

	q_mut.lock();
      }

      swap(thread_queue,n_thread_queue);

    }catch(...){
      /* this one is IMHO very important, as lock is called in try block! */
      ERROR("unexpected exception, state may be invalid!\n");
    }

    bool more = !thread_queue.empty();
    q_mut.unlock();

    DBG("Thread watcher finished\n");
    if(!more)
      _run_cond.set(false);
  }
}

