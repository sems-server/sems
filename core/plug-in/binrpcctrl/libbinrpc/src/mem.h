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


#ifndef __BRPC_MEM_H__
#define __BRPC_MEM_H__

#include <stddef.h>

#ifdef _LIBBINRPC_BUILD
void *(*brpc_calloc)(size_t nmemb, size_t size);
void *(*brpc_malloc)(size_t size);
void (*brpc_free)(void *brpc_ptr);
void *(*brpc_realloc)(void *ptr, size_t size);
#endif /* _LIBBINRPC_BUILD */


/**
 * API calls.
 */

void brpc_mem_setup(
		void* (*c)(size_t,size_t),
		void* (*m)(size_t),
		void (*f)(void *),
		void* (*r)(void *,size_t));

#endif /* __BRPC_MEM_H__ */

