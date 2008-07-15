/*
 * $Id$
 *
 * Copyright (C) 2007 Raphael Coeffic
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


// This timer is not defined by
// RFC 3261. But it is needed
// to handle 200 ACKs automatically
// in UAC transactions.

//type 0x0b
#define L_TIMER  64*T1_TIMER

#endif


/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
