#ifndef _singleton_h_
#define _singleton_h_

#include "AmThread.h"

template<class T>
class singleton
  : public T
{
  singleton() : T() {}
  ~singleton() {}
  
public:
  static singleton<T>* instance() 
  {
    _inst_m.lock();
    if(!_instance) {
      _instance = new singleton<T>();
    }

    singleton<T>* ret = _instance;
    _inst_m.unlock();

    return ret;
  }
  
  static void dispose() 
  {
    _inst_m.lock();
    _instance->dispose();
    delete _instance;
    _instance = NULL;
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
