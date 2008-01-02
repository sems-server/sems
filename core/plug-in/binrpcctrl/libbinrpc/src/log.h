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


#ifndef __BRPC_LOG_H__
#define __BRPC_LOG_H__


#ifdef _LIBBINRPC_BUILD

#include <syslog.h>
#include "config.h"

#define LOG(priority, msg, args...) \
		brpc_syslog(priority, msg, ##args)

#define _STRINGIFY(i) #i
#define STRINGIFY(i) _STRINGIFY(i)

#define REL_REF \
		"[" BINRPC_LIB_VER "]: "
#define CODE_REF \
		"[" __FILE__ ":" STRINGIFY(__LINE__) "]: "

#define LOG_REF	CODE_REF

#define ERR(msg, args...)	LOG(LOG_ERR, "ERROR " LOG_REF msg, ##args)
#define WARN(msg, args...)	LOG(LOG_WARNING, "WARNING " LOG_REF msg, ##args)
#define INFO(msg, args...)	LOG(LOG_INFO, "INFO " LOG_REF msg, ##args)
#define BUG(msg, args...)	ERR("### BUG ### " msg, ##args)

#if defined NDEBUG
#define DBG(msg, args...)	/* no debug */
#else
#define DBG(msg, args...)	\
		LOG(LOG_DEBUG, "--- debug --- %s" CODE_REF msg, __FUNCTION__, ##args)
#endif /* NDEBUG */


void (*brpc_syslog)(int priority, const char *format, ...);

#endif /* _LIBBINRPC_BUILD */


/**
 * API calls.
 */

void brpc_log_setup(void (*s)(int , const char *, ...));

#endif /* __BRPC_LOG_H__ */

