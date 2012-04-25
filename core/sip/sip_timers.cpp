/*
 * Copyright (C) 2012 Stefan Sayer
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
#include "sip_timers.h"

unsigned int sip_timers[n_sip_timers] = {
  DEFAULT_A_TIMER,
  DEFAULT_B_TIMER,
  DEFAULT_C_TIMER,
  DEFAULT_D_TIMER,
  DEFAULT_E_TIMER,
  DEFAULT_F_TIMER,
  DEFAULT_G_TIMER,
  DEFAULT_H_TIMER,
  DEFAULT_I_TIMER,
  DEFAULT_J_TIMER,
  DEFAULT_K_TIMER,
  DEFAULT_L_TIMER,
  DEFAULT_M_TIMER
};

unsigned int sip_timer_t2 = DEFAULT_T2_TIMER;
