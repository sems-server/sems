#ifndef _SBCCallControlAPI_h_
#define _SBCCallControlAPI_h_

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

#endif
