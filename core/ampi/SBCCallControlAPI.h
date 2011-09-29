#ifndef _SBCCallControlAPI_h_
#define _SBCCallControlAPI_h_

#define CC_INTERFACE_MAND_VALUES_METHOD "getMandatoryValues"


#define CC_API_PARAMS_LTAG              0
#define CC_API_PARAMS_CALL_PROFILE      1
#define CC_API_PARAMS_TIMESTAMPS        2

#define CC_API_PARAMS_CFGVALUES         3
#define CC_API_PARAMS_TIMERID           4

#define CC_API_PARAMS_OTHERID           3

#define CC_API_TS_START_SEC             0
#define CC_API_TS_START_USEC            1
#define CC_API_TS_CONNECT_SEC           2
#define CC_API_TS_CONNECT_USEC          3
#define CC_API_TS_END_SEC               4
#define CC_API_TS_END_USEC              5

#define SBC_CC_DROP_ACTION              0
#define SBC_CC_REFUSE_ACTION            1
#define SBC_CC_SET_CALL_TIMER_ACTION    2

// index in action parameter:
#define SBC_CC_ACTION              0

//    refuse with
#define SBC_CC_REFUSE_CODE         1
#define SBC_CC_REFUSE_REASON       2
#define SBC_CC_REFUSE_HEADERS      3

//     set timer
#define SBC_CC_TIMER_TIMEOUT       1

/** post an SBCCallTimerEvent to an SBC call in order to set or reset call timer */
#define SBCCallTimerEvent_ID -563
struct SBCCallTimerEvent : public AmEvent {
  enum TimerAction {
    Remove = 0,
    Set,
    Reset
  };

  TimerAction timer_action;
  int timeout;
  int timer_id;

 SBCCallTimerEvent(TimerAction timer_action, int timer_id, int timeout = 0)
    : AmEvent(SBCCallTimerEvent_ID),
    timer_id(timer_id), timer_action(timer_action), timeout(timeout) { }
};

#endif
