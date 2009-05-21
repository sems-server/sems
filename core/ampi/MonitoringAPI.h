
#ifndef _MONITORING_API_H
#define _MONITORING_API_H

/*
 * macros to make monitoring easy to use and not 
 * mess up source code too much
 */

#ifdef USE_MONITORING				

#define MONITORING_LOG(callid, property, value)	\
  if (NULL != AmSessionContainer::monitoring_di) {			\
    AmArg di_args,ret;							\
    di_args.push(AmArg(callid));					\
    di_args.push(AmArg(property));					\
    di_args.push(AmArg(value));						\
    AmSessionContainer::monitoring_di->invoke("log", di_args, ret);	\
  }									\
  
  // hm... there must be a better method for this...
#define MONITORING_LOG2(AmSessionContainer, callid, prop1, val1, prop2, val2) \
  if (NULL != AmSessionContainer::monitoring_di) {			\
    AmArg di_args,ret;							\
    di_args.push(AmArg(callid));					\
    di_args.push(AmArg(prop1));						\
    di_args.push(AmArg(val1));						\
    di_args.push(AmArg(prop2));						\
    di_args.push(AmArg(val2));						\
    AmSessionContainer::monitoring_di->invoke("log", di_args, ret);	\
  }									\

#define MONITORING_LOG3(callid, prop1, val1, prop2, val2, prop3, val3)	\
  if (NULL != AmSessionContainer::monitoring_di) {			\
    AmArg di_args,ret;							\
    di_args.push(AmArg(callid));					\
    di_args.push(AmArg(prop1));						\
    di_args.push(AmArg(val1));						\
    di_args.push(AmArg(prop2));						\
    di_args.push(AmArg(val2));						\
    di_args.push(AmArg(prop3));						\
    di_args.push(AmArg(val3));						\
    AmSessionContainer::monitoring_di->invoke("log", di_args, ret);	\
  }									\

#define MONITORING_LOG4(callid, prop1, val1, prop2, val2, prop3, val3, prop4, val4) \
  if (NULL != AmSessionContainer::monitoring_di) {			\
    AmArg di_args,ret;							\
    di_args.push(AmArg(callid));					\
    di_args.push(AmArg(prop1));						\
    di_args.push(AmArg(val1));						\
    di_args.push(AmArg(prop2));						\
    di_args.push(AmArg(val2));						\
    di_args.push(AmArg(prop3));						\
    di_args.push(AmArg(val3));						\
    di_args.push(AmArg(prop4));						\
    di_args.push(AmArg(val4));						\
    AmSessionContainer::monitoring_di->invoke("log", di_args, ret);	\
  }									\

#define MONITORING_LOG5(callid, prop1, val1, prop2, val2, prop3, val3, prop4, val4, prop5, val5) \
  if (NULL != AmSessionContainer::monitoring_di) {			\
    AmArg di_args,ret;							\
    di_args.push(AmArg(callid));					\
    di_args.push(AmArg(prop1));						\
    di_args.push(AmArg(val1));						\
    di_args.push(AmArg(prop2));						\
    di_args.push(AmArg(val2));						\
    di_args.push(AmArg(prop3));						\
    di_args.push(AmArg(val3));						\
    di_args.push(AmArg(prop4));						\
    di_args.push(AmArg(val4));						\
    di_args.push(AmArg(prop5));						\
    di_args.push(AmArg(val5));						\
    AmSessionContainer::monitoring_di->invoke("log", di_args, ret);	\
  }									\
  
#define MONITORING_LOG_ADD(callid, property, value)			\
  if (NULL != AmSessionContainer::monitoring_di) {			\
    AmArg di_args,ret;							\
    di_args.push(AmArg(callid));					\
    di_args.push(AmArg(property));					\
    di_args.push(AmArg(value));						\
    AmSessionContainer::monitoring_di->invoke("logAdd", di_args, ret);	\
  }									\

#define MONITORING_MARK_FINISHED(callid)				\
  if (NULL != AmSessionContainer::monitoring_di) {			\
    AmArg di_args,ret;							\
    di_args.push(AmArg(callid));					\
    AmSessionContainer::monitoring_di->invoke("markFinished", di_args, ret); \
  }									\

// it is always using AmSessionContainer::monitoring_di, thus these should 
// not be necessary in apps

#define _MONITORING_DECLARE_INTERFACE(MOD)	\
  AmDynInvoke* MOD::monitoring_di = 0;		\

#define _MONITORING_DEFINE_INTERFACE		\
  static AmDynInvoke*  monitoring_di;		\

#define MONITORING_GLOBAL_INTERFACE		\
  AmSessionContainer::monitoring_di

#define _MONITORING_INIT						\
  AmDynInvokeFactory* monitoring_fact= AmPlugIn::instance()->getFactory4Di("monitoring"); \
  if(!monitoring_fact) {						\
    INFO("monitoring module not loaded, monitoring disabled\n");	\
  } else {								\
    monitoring_di = monitoring_fact->getInstance();			\
    assert(monitoring_di);						\
    INFO("monitoring enabled\n");					\
  }									\

#else

#define _MONITORING_DECLARE_INTERFACE(MOD) 
#define _MONITORING_DEFINE_INTERFACE		
#define _MONITORING_INIT							
#define MONITORING_LOG(callid, property, value)	
#define MONITORING_LOG2(callid, prop1, val1, prop2, val2)	
#define MONITORING_LOG3(callid, prop1, val1, prop2, val2, prop3, val3)	
#define MONITORING_LOG4(callid, prop1, val1, prop2, val2, prop3, val3, prop4, val4) 
#define MONITORING_LOG5(callid, prop1, val1, prop2, val2, prop3, val3, prop4, val4, prop5, val5) 
#define MONITORING_LOG_ADD(callid, property, value)
#define MONITORING_MARK_FINISHED(callid)	

#endif

#endif
