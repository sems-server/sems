/**
 *  Copyright (C) 2007 iptego GmbH
 *
 *  This file is part of libbinrpc.
 *
 *  libbinrpc is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  libbinrpc is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __BRPC_TIME_H__
#define __BRPC_TIME_H__

/* for us resolution */
typedef unsigned long brpc_tv_t;

typedef brpc_tv_t (*brpc_now_f)(void);

void brpc_ticker_setup(brpc_now_f fn);

#ifdef _LIBBINRPC_BUILD
extern brpc_now_f brpc_now;
#endif

#endif /* __BRPC_TIME_H__ */
