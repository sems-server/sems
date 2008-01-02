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


#include <string.h>
#include <stdio.h>
#include "errno.h"


__brpc_tls int brpc_errno = 0;

#define ERR_BUFF_SIZE	1024

#ifndef NDEBUG
__brpc_tls char *brpc_efile = __FILE__;
__brpc_tls int brpc_eline = 0;
#endif /* NDEBUG */

char *brpc_strerror()
{
	static __brpc_tls char buff[ERR_BUFF_SIZE];
	char *msg;
	switch (brpc_errno) {
		case ELOCK: msg = "Locking subsystem error"; break;
		case ERESLV: msg = "DNS resolution failure"; break;
		case EFMT: msg = "Descriptor - structure missmatch"; break;
#ifdef BINRPC_REENTRANT
		default: msg = strerror_r(brpc_errno); break;
#else
		default: msg = strerror(brpc_errno); break;
#endif
	}
#ifndef NDEBUG
	snprintf(buff, ERR_BUFF_SIZE, "%s [%s:%d]", msg, brpc_efile, brpc_eline);
#else
	snprintf(buff, ERR_BUFF_SIZE, "%s", msg);
#endif
	return buff;
}

