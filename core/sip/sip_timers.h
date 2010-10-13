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

    STIMER_INVALID=0,

    // INVITE client transaction
    STIMER_A,  // Calling: (re-)send INV
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
    STIMER_M
};


/**
 * SIP transaction timer default values
 */

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


// Following timer values are not defined by
// RFC 3261.

// Used to handle 200 ACKs automatically
// in INVITE client transactions.
//type 0x0b
#define L_TIMER  64*T1_TIMER

// Transport address failover timer:
// - used to cycle throught multiple addresses
//   in case the R-URI resolves to multiple addresses
#define M_TIMER  (B_TIMER/4)

#endif


/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
