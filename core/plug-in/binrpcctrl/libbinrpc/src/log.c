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


#include "log.h"

static void black_hole(int prio, const char *fmt, ...) {}

#ifdef USE_DEFAULT_SYSLOG
void (*brpc_syslog)(int priority, const char *format, ...) = syslog;
#else
void (*brpc_syslog)(int priority, const char *format, ...) = black_hole;
#endif

void brpc_log_setup(void (*s)(int , const char *, ...))
{
	brpc_syslog = s;
}

