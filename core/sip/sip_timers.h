/*
 * $Id: sip_timers.h 1048 2008-07-15 18:48:07Z sayer $
 *
 * Copyright (C) 2007 Raphael Coeffic
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
#ifndef _sip_timers_h_
#define _sip_timers_h_

/**
 * SIP transaction timer type definition
 */
enum sip_timer_type {

    // INVITE client transaction
    STIMER_A=0,// Calling: (re-)send INV
    STIMER_B,  // Calling -> Terminated
    STIMER_D,  // Completed -> Terminated

    // non-INVITE client transaction
    STIMER_E,  // Trying/Proceeding: (re-)send request
    STIMER_F,  // Trying/Proceeding -> Terminated
    STIMER_K,  // Completed -> Terminated

    // INVITE server transaction
    STIMER_G,  // Completed: (re-)send response
    STIMER_H,  // Completed -> Terminated
    STIMER_I,  // Confirmed -> Terminated

    // non-INVITE server transaction
    STIMER_J,  // Completed -> Terminated

    // These timers are not defined by
    // RFC 3261. 

    // Used to handle 200 ACKs automatically
    // in INVITE client transactions.
    STIMER_L,  // Terminated_200 -> Terminated

    // Transport address failover timer:
    // - used to cycle throught multiple addresses
    //   in case the R-URI resolves to multiple addresses
    STIMER_M,

    // INVITE client transaction
    STIMER_C,  // Proceeding -> Terminated

    // Blacklist grace timer
    STIMER_BL,

    __STIMER_MAX
};


/**
 * SIP transaction timer default values
 */

#define T1_TIMER  500 /* 500 ms */
#define DEFAULT_T2_TIMER 4000 /*   4 s  */
#define T4_TIMER 5000 /*   5 s  */

//type 0x01
#define DEFAULT_A_TIMER  T1_TIMER

//type 0x02
#define DEFAULT_B_TIMER  64*T1_TIMER

//type 0x0d
#define DEFAULT_C_TIMER  (3*60*1000)

//type 0x03
#define DEFAULT_D_TIMER  64*T1_TIMER

//type 0x04
#define DEFAULT_E_TIMER  T1_TIMER

//type 0x05
#define DEFAULT_F_TIMER  64*T1_TIMER

//type 0x06
#define DEFAULT_K_TIMER  T4_TIMER

//type 0x07
#define DEFAULT_G_TIMER  T1_TIMER

//type 0x08
#define DEFAULT_H_TIMER  64*T1_TIMER

//type 0x09
#define DEFAULT_I_TIMER  T4_TIMER

//type 0x0a
#define DEFAULT_J_TIMER  64*T1_TIMER

// Following timer values are not defined by
// RFC 3261.

// Used to handle 200 ACKs automatically
// in INVITE client transactions.
//type 0x0b
#define DEFAULT_L_TIMER  64*T1_TIMER

// Transport address failover timer:
// - used to cycle throught multiple addresses
//   in case the R-URI resolves to multiple addresses
//type 0x0c
#define DEFAULT_M_TIMER  (DEFAULT_B_TIMER/4)

// Blacklist grace timer (client transaction only)
// - set after locally generated 408
//   to wait for downstream 408
#define DEFAULT_BL_TIMER DEFAULT_B_TIMER

#define A_TIMER sip_timers[STIMER_A]
#define B_TIMER sip_timers[STIMER_B]
#define D_TIMER sip_timers[STIMER_D]

#define E_TIMER sip_timers[STIMER_E]
#define F_TIMER sip_timers[STIMER_F]
#define K_TIMER sip_timers[STIMER_K]

#define G_TIMER sip_timers[STIMER_G]
#define H_TIMER sip_timers[STIMER_H]
#define I_TIMER sip_timers[STIMER_I]

#define J_TIMER sip_timers[STIMER_J]

#define L_TIMER sip_timers[STIMER_L]
#define M_TIMER sip_timers[STIMER_M]
#define C_TIMER sip_timers[STIMER_C]

#define BL_TIMER sip_timers[STIMER_BL]

extern unsigned int sip_timers[__STIMER_MAX];

#define T2_TIMER sip_timer_t2
extern unsigned int sip_timer_t2;

const char* timer_name(unsigned int type);

#endif

/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
