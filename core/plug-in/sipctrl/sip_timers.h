#ifndef _sip_timers_h_
#define _sip_timers_h_

#define T1_TIMER  500 /* 500 ms */
#define T2_TIMER 4000 /*   4 s  */
#define T4_TIMER 5000 /*   5 s  */

//type 0x01
#define A_TIMER  T1_TIMER
//type 0x02
#define B_TIMER  64*T1_TIMER

//type 0x03
#define D_TIMER  64*T1_TIMER

//type 0x04
#define E_TIMER  T1_TIMER
//type 0x05
#define F_TIMER  64*T1_TIMER

//type 0x06
#define K_TIMER  T4_TIMER


//type 0x07
#define G_TIMER  T1_TIMER
//type 0x08
#define H_TIMER  64*T1_TIMER

//type 0x09
#define I_TIMER  T4_TIMER

//type 0x0a
#define J_TIMER  64*T1_TIMER

#endif
