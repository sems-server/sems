#ifndef _SBCCallControlAPI_h_
#define _SBCCallControlAPI_h_

#include "AmEvent.h"

#define CC_INTERFACE_MAND_VALUES_METHOD "getMandatoryValues"


#define CC_API_PARAMS_CC_NAMESPACE      0
#define CC_API_PARAMS_LTAG              1
#define CC_API_PARAMS_CALL_PROFILE      2
#define CC_API_PARAMS_SIP_MSG           3
#define CC_API_PARAMS_TIMESTAMPS        4

#define CC_API_PARAMS_CFGVALUES         5
#define CC_API_PARAMS_TIMERID           6

#define CC_API_PARAMS_OTHERID           5

#define CC_API_TS_START_SEC             0
#define CC_API_TS_START_USEC            1
#define CC_API_TS_CONNECT_SEC           2
#define CC_API_TS_CONNECT_USEC          3
#define CC_API_TS_END_SEC               4
#define CC_API_TS_END_USEC              5

#define SBC_CC_DROP_ACTION              0
#define SBC_CC_REFUSE_ACTION            1
#define SBC_CC_SET_CALL_TIMER_ACTION    2

#define SBC_CC_REPL_SET_GLOBAL_ACTION        10
#define SBC_CC_REPL_REMOVE_GLOBAL_ACTION     11

// index in action parameter:
#define SBC_CC_ACTION              0

//    refuse with
#define SBC_CC_REFUSE_CODE         1
#define SBC_CC_REFUSE_REASON       2
#define SBC_CC_REFUSE_HEADERS      3

//     set timer
#define SBC_CC_TIMER_TIMEOUT       1

//     set/remove globals
#define SBC_CC_REPL_SET_GLOBAL_SCOPE 1
#define SBC_CC_REPL_SET_GLOBAL_NAME  2
#define SBC_CC_REPL_SET_GLOBAL_VALUE 3

/** post an SBCCallTimerEvent to an SBC call in order to set or reset call timer */
#define SBCCallTimerEvent_ID -563
struct SBCCallTimerEvent : public AmEvent {
  enum TimerAction {
    Remove = 0,
    Set,
    Reset
  };

  TimerAction timer_action;
  double timeout;
  int timer_id;

 SBCCallTimerEvent(TimerAction timer_action, int timer_id, double timeout = 0)
    : AmEvent(SBCCallTimerEvent_ID),
    timer_action(timer_action), timeout(timeout), timer_id(timer_id) { }
};

#define SBCControlEvent_ID -564
struct SBCControlEvent : public AmEvent {
  string cmd;
  AmArg params;

  SBCControlEvent(const string& cmd, const AmArg& params)
    : AmEvent(SBCControlEvent_ID), cmd(cmd), params(params) { }

  SBCControlEvent(const string& cmd)
    : AmEvent(SBCControlEvent_ID), cmd(cmd) { }

};

#endif
