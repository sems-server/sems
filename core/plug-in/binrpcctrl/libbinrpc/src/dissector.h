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

#ifndef __BINRPC_DISSECTOR_H__
#define __BINRPC_DISSECTOR_H__

#include <stdbool.h>
#include "list.h"
#include "call.h"

enum BRPC_DISSECTOR_TYPE {
	BRPC_MSG_DISSECTOR,
	BRPC_VAL_DISSECTOR
};

typedef struct {
	struct brpc_list_head **stack; /**< every push inserts here */
	size_t cap; /**< how many slots in stack */
	ssize_t top; /**< where to make an insertion */
	struct brpc_list_head *cursor; /**< current position in current level */
	enum BRPC_DISSECTOR_TYPE type;
	union {
		brpc_t *msg; /**< original message */
		brpc_val_t *val; /* is it really needed? */
	};
	/* TODO: this practically doubles values tree => use one tree (?) */
	struct brpc_list_head list; /* refs to siblings */
	struct brpc_list_head head; /* refs to children */
} brpc_dissect_t;

brpc_dissect_t *brpc_msg_dissector(brpc_t *msg);
brpc_dissect_t *brpc_val_dissector(brpc_val_t *val);
void brpc_dissect_free(brpc_dissect_t *diss);
/**
 * Step into current RPC value; it can only succeed for set values (arrays,
 * maps, AVPs).
 */
bool brpc_dissect_in(brpc_dissect_t *diss);
/**
 * Step out into previous level.
 */
bool brpc_dissect_out(brpc_dissect_t *diss);
/**
 * Checks if there is a new value to inspect in current level.
 */
bool brpc_dissect_next(brpc_dissect_t *diss);
/**
 * Returns the number of RPC values in current level.
 */
size_t brpc_dissect_cnt(brpc_dissect_t *diss);
/**
 * Returns the current BINRPC value type.
 */
brpc_vtype_t brpc_dissect_seqtype(brpc_dissect_t *diss);
bool brpc_dissect_chain(brpc_dissect_t *anchor, brpc_dissect_t *cell);
size_t brpc_dissect_levcnt(brpc_dissect_t *diss);

#define brpc_dissect_fetch(_diss_)	\
		(const brpc_val_t *)_BRPC_VAL4LIST((_diss_)->cursor)

#define brpc_dissect_level(_diss_)	((const ssize_t)((_diss_)->top - 1))


#endif /* __BINRPC_DISSECTOR_H__ */
