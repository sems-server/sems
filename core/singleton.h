#ifndef _singleton_h_
#define _singleton_h_

#include "AmThread.h"

template<class T>
class singleton
  : public T
{
  singleton() : T() {}
  ~singleton() {}

  // T may define on_singleton_created(), which is called once the whole
  // singleton<T> exists - e.g. to start the instance's thread. T's own
  // constructor is too early for that: singleton<T>'s constructor runs after
  // it and rewrites the object's vtable pointer, while the new thread may
  // already be making virtual calls through it.
  template<class U>
  static auto created(U* u, int) -> decltype(u->on_singleton_created(), void())
  {
    u->on_singleton_created();
  }

  template<class U>
  static void created(U*, long) {}

public:
  static singleton<T>* instance() 
  {
    _inst_m.lock();
    if(NULL == _instance) {
      _instance = new singleton<T>();
      created<T>(_instance, 0);
    }
    _inst_m.unlock();

    return _instance;
  }

  static bool haveInstance()
  {
    bool res = false;
    _inst_m.lock();
    res = _instance != NULL;
    _inst_m.unlock();
    return res;
  }
  
  static void dispose() 
  {
    _inst_m.lock();
    if(_instance != NULL){
      _instance->T::dispose();
      delete _instance;
      _instance = NULL;
    }
    _inst_m.unlock();
  }

private:
  static singleton<T>* _instance;
  static AmMutex       _inst_m;
};

template<class T>
singleton<T>* singleton<T>::_instance = NULL;

template<class T>
AmMutex singleton<T>::_inst_m;

#endif
