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


#ifndef __BRPC_ERRNO_H__
#define __BRPC_ERRNO_H__

#ifdef _LIBBINRPC_BUILD

#include <limits.h>
#include <errno.h>

#include "tls.h"

#define ELOCK	(INT_MAX - 1)
#define ERESLV	(INT_MAX - 2)
#define EFMT	(INT_MAX - 3)

#ifndef NDEBUG

extern __brpc_tls char *brpc_efile;
extern __brpc_tls int brpc_eline;

#define WERRNO(_errno_) \
	do { \
		brpc_errno = _errno_; \
		brpc_efile = __FILE__; \
		brpc_eline = __LINE__; \
	} while (0)

#define WSYSERRNO	WERRNO(errno)

#else /* NDEBUG */

#define WERRNO(_errno_) \
		brpc_errno = _errno_

#define WSYSERRNO	WERRNO(errno)

#endif /* NDEBUG */

#endif /* _LIBBINRPC_BUILD */

extern __brpc_tls int brpc_errno;
char *brpc_strerror();


#endif /* __BRPC_ERRNO_H__ */

