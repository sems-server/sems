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

#include <sys/time.h>

#include "misc.h"
#include "time.h"

__LOCAL brpc_tv_t _brpc_now();

brpc_now_f brpc_now = _brpc_now;

void brpc_ticker_setup(brpc_now_f fn) { brpc_now = fn; }

__LOCAL brpc_tv_t _brpc_now()
{
	struct timeval tv;
	brpc_tv_t now;
	gettimeofday(&tv, NULL);
	now = tv.tv_sec;
	now *= 1000000LU;
	now += tv.tv_usec;
	return now;
}
