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

#ifndef __BINRPC_LOCK_H__
#define __BINRPC_LOCK_H__

typedef void* brpc_lock_t;

/**
 * Primitive type to alloc&init a new lock. 
 * @return The lock, in state unlocked, or NULL on error.
 */
typedef brpc_lock_t *(*brpc_lock_new_f)(void);
/**
 * Primitive type to acquire a lock.
 * @return 0 for success, negative otherwise.
 */
typedef int (*brpc_lock_get_f)(brpc_lock_t *);
/**
 * Primitive type to release a lock.
 * @return 0 for success, negative otherwise.
 */
typedef int (*brpc_lock_let_f)(brpc_lock_t *);
/**
 * Primitive type to release any lock resources.
 * @return 0 for success, negative otherwise.
 */
typedef int (*brpc_lock_del_f)(brpc_lock_t *);
/**
 * Primitive type to release 
 */

enum BINRPC_LOCKING_MODEL {
	BRPC_LOCK_PROCESS,
	BRPC_LOCK_THREAD,
};

/**
 * Register locking primitives.
 * Default is RT library locking.
 * @param locksz Number of bytes (including possible allignment padding) used
 * to store a lock object in memory.
 */
void brpc_locking_setup(
		brpc_lock_new_f n,
		brpc_lock_get_f a,
		brpc_lock_let_f r,
		brpc_lock_del_f d);

/**
 * Set the locking model (inter-thread/inter-process).
 * The default is inter-process.
 * The call only makes sense if default (RT library) locking is used.
 */
void brpc_locking_model(enum BINRPC_LOCKING_MODEL model);


#ifdef _LIBBINRPC_BUILD

extern brpc_lock_new_f brpc_lock_new;
extern brpc_lock_get_f brpc_lock_get;
extern brpc_lock_let_f brpc_lock_let;
extern brpc_lock_del_f brpc_lock_del;

#endif /* _LIBBINRPC_BUILD */


#endif /* __BINRPC_LOCK_H__ */
