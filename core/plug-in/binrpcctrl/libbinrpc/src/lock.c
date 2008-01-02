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

#ifndef NOP_DFL_LOCKS
#include <semaphore.h>
#endif

#include "mem.h"
#include "errno.h"
#include "lock.h"

/* forward declaration */
static brpc_lock_t *_brpc_lock_new(void);
static int _brpc_loc_del(brpc_lock_t *lock);
#ifdef NOP_DFL_LOCKS
static int _brpc_lock_get(brpc_lock_t *lock);
static int _brpc_lock_let(brpc_lock_t *lock);
#endif /* NOP_DFL_LOCKS */

static int locking_model = BRPC_LOCK_PROCESS;

brpc_lock_new_f brpc_lock_new = _brpc_lock_new;
#ifndef NOP_DFL_LOCKS
brpc_lock_get_f brpc_lock_get = (brpc_lock_get_f)sem_wait;
brpc_lock_let_f brpc_lock_let = (brpc_lock_let_f)sem_post;
#else /* NOP_DFL_LOCKS */
brpc_lock_get_f brpc_lock_get = _brpc_lock_get;
brpc_lock_let_f brpc_lock_let = _brpc_lock_let;
#endif /* NOP_DFL_LOCKS */
brpc_lock_del_f brpc_lock_del = _brpc_loc_del;


void brpc_locking_setup(
		brpc_lock_new_f n,
		brpc_lock_get_f a,
		brpc_lock_let_f r,
		brpc_lock_del_f d)
{
	brpc_lock_new = n;
	brpc_lock_get = a;
	brpc_lock_let = r;
	brpc_lock_del = d;
}

void brpc_locking_model(enum BINRPC_LOCKING_MODEL model)
{
	locking_model = model;
}

#ifndef NOP_DFL_LOCKS
static brpc_lock_t *_brpc_lock_new(void)
{
	brpc_lock_t *lock;
	lock = brpc_calloc(1, sizeof(sem_t));
	if (! lock) {
		WERRNO(ENOMEM);
		return NULL;
	}
	if (sem_init((sem_t *)lock, locking_model == BRPC_LOCK_PROCESS, 
			/* semaphore unlocked */1) == -1) {
		WSYSERRNO;
		goto error;
	}
	
	return lock;
error:
	brpc_free(lock);
	return NULL;
}

static int _brpc_loc_del(brpc_lock_t *lock)
{
	if (! lock) {
		WERRNO(EINVAL);
		return -1;
	}
	if (sem_destroy((sem_t *)lock) == -1) {
		WSYSERRNO;
		return -1;
	}
	brpc_free(lock);
	return 0;
}

#else /* NOP_DFL_LOCKS */


static brpc_lock_t *_brpc_lock_new(void) { return (brpc_lock_t *)-1; }
static int _brpc_lock_get(brpc_lock_t *lock) { return 0; }
static int _brpc_lock_let(brpc_lock_t *lock) { return 0; }
static int _brpc_loc_del(brpc_lock_t *lock) { return 0; }

#endif /* NOP_DFL_LOCKS */

