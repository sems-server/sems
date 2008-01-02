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


/*
 * Record format:
 *  1b    3b     4b      [size] B     [size] or [len field] Bytes
 * | S | size | type | <len field> |            value             |
 *
 * if S==0, size is the size (in bytes) of the value (if size==0 => null value)
 * if S==1, optional_value_len is present, and size is it's size
 *    (if size==0 => and type==array or struct => marks end, else
 *     error, reserved)
 */

#ifndef __BRPC_VALUE_H__
#define __BRPC_VALUE_H__

#include <inttypes.h>
#include <unistd.h> /* ssize_t */
#include <string.h>
#include <stdbool.h>

#include "list.h"

#ifdef _LIBBINRPC_BUILD
#define REC_HAVE_SFLD	0x80
#define REC_SIZE_BITS	3
#define REC_TYPE_BITS	4
#define REC_SIZE_OFF	4
#define REC_HDR_SIZE	1U

#define MAX_REC_SSIZE	((1 << REC_SIZE_BITS) - 1)
#define REC_SSIZE_MASK	MAX_REC_SSIZE
#endif /* _LIBBINRPC_BUILD */

#ifdef BINRPC_VER1_COMPAT
typedef enum {
	BRPC_VAL_INT		= 0,
	BRPC_VAL_STR		= 1,
	BRPC_VAL_DOUBLE		= 2,
	BRPC_VAL_BIN		= 6,
	BRPC_VAL_AVP		= 5,
	BRPC_VAL_MAP		= 3,
	BRPC_VAL_LIST		= 4,
	BRPC_VAL_NONE		= 7/* keep last (Note: this is not NULL!) */
} brpc1_vtype_t; /* type of types */
#endif /* BINRPC_VER1_COMPAT */

typedef enum {
	BRPC_VAL_RESERVED	= 0,
	/* complex types */
	BRPC_VAL_LIST		= 1,
	BRPC_VAL_AVP		= 2,
	BRPC_VAL_MAP		= 3,
	/* basic types */
	BRPC_VAL_INT		= 11,
	BRPC_VAL_FLOAT		= 12,
	BRPC_VAL_STR		= 13,
	BRPC_VAL_BIN		= 14,
	/* complex type: the container is the message */
	BRPC_VAL_NONE	/* keep last (Note: this is not NULL!) */
} brpc_vtype_t; /* type of types */

/*
 * Basic types
 */
typedef int32_t brpc_int_t;

typedef struct {
	char *val;
	size_t len;
} brpc_str_t;

typedef struct {
	uint8_t *val;
	size_t len;
} brpc_bin_t;

/*
 * BINRPC value structure.
 */
typedef struct brpc_rec_struct {
	brpc_vtype_t type;
	bool locked; /**< is the record received? (<>locally generated)*/
	bool null;
	union {
		brpc_int_t int32;
		brpc_bin_t bin;
		brpc_str_t str;
		struct {
			struct brpc_list_head list;
			size_t cnt; /* how many top level values in list */
		} seq; /*: avp, map, list */
	} val;
	struct brpc_list_head list; /* sibling anchor */
} brpc_val_t;


brpc_val_t *brpc_seq(brpc_vtype_t type, ...);

#define brpc_seq_cells(_ptr_)	(_ptr_)->val.seq.list
#define _BRPC_VAL4LIST(_list_)	brpc_list_entry(_list_, brpc_val_t, list)
#define _BRPC_VAL4SEQ(_list_)	\
		brpc_list_entry(_list_, brpc_val_t, val.seq.list)


#ifdef _LIBBINRPC_BUILD
brpc_val_t *brpc_val_deserialize(uint8_t **, const uint8_t *);
uint8_t* brpc_val_serialize(brpc_val_t *, uint8_t *, const uint8_t *);
bool brpc_vals_repr(struct brpc_list_head *head, brpc_str_t *buff, 
		ssize_t *pos);

#define brpc_seqval_list_for_each(_pos_, _vhead_)	\
		list_for_each_entry(_pos_, _vhead_, list)
#define brpc_seqval_for_each(_pos_, _val_)	\
		brpc_seqval_list_for_each(_pos_, &(_val_)->val.seq.list)
#define brpc_seqval_for_each_safe(_pos_, _val_, _tmp_)	\
		list_for_each_entry_safe(_pos_, _tmp_, &(_val_)->val.seq.list, list)

#endif /* _LIBBINRPC_BUILD */


/*
 * API CALLS.
 */

/*
 * Build records (arguments/results).
 */

/**
 * Free a BINRPC value.
 */
void brpc_val_free(brpc_val_t *rec);
/**
 * Build a BINRPC NULL value of given type.
 * @param type Type of NULL value.
 * @return brpc_val_t* NULL value.
 */
brpc_val_t *brpc_null(brpc_vtype_t type);
/**
 * Build a BINRPC int type value.
 * @param val Integer value to assign.
 * @return brpc_val_t* int value.
 */
brpc_val_t *brpc_int(brpc_int_t val);
/**
 * Build a BINRPC binary value.
 * @param val The sequence of bytes to be assigned.
 * @param len Length of the 'val' sequence.
 * @return brpc_val_t* bin value.
 */
brpc_val_t *brpc_bin(uint8_t *val, size_t len);
/**
 * Build a BINRPC character string value.
 * @param val The sequence of characters, *not* necessarily 0-terminated.
 * @param len The lenght of this sequence (including 0-term, if present).
 * @return brpc_val_t* string value.
 * @see brpc_cstr
 */
brpc_val_t *brpc_str(char *val, size_t len);
/**
 * Build a BINRPC AVP value.
 * @param name Attribute's identifier.
 * @param value Attribute's value.
 * @return brpc_val_t* AVP value.
 */
brpc_val_t *brpc_avp(brpc_val_t *name, brpc_val_t *value);
/**
 * Adds a BINRPC value to a BINRPC list.
 * @param list The BINRPC list to add the value to.
 * @param val The BINRPC value to add.
 * @return Status of operation.
 */
bool brpc_list_add(brpc_val_t *list, brpc_val_t *val);
/**
 * Adds a new AVP to a BINRPC map value.
 * @param map The BINRPC map value.
 * @param avp The BINRPC AVP to add to map.
 * @return Status of operation.
 */
bool brpc_map_add(brpc_val_t *map, brpc_val_t *avp);
/**
 * Adds a member to an AVP.
 * @param member AVP identifier (if first add) or value (if second).
 * @return Status of operation.
 */
bool brpc_avp_add(brpc_val_t *avp, brpc_val_t *member);
/**
 * Deep clone a value.
 * @param orig Value to be cloned;
 * @return A value identical with original.
 */
brpc_val_t *brpc_val_clone(const brpc_val_t *orig);
/**
 * Build a BINRPC character string value.
 * @param _cptr_ The sequence of characters, 0-terminated.
 * @return brpc_val_t* string value.
 * @see brpc_str
 */
#define brpc_cstr(_cptr_)	brpc_str(_cptr_, strlen(_cptr_) + 1)
/**
 * Build a BINRPC character string value.
 * @param _cptr_ brpc_str_t structure.
 * @return brpc_val_t* string value.
 * @see brpc_str
 */
#define brpc_bstr(_bstr_)	brpc_str((_bstr_)->val, (_brpc_)->val)
/**
 * Builds a BINRPC list value.
 * @param args List of brpc_val_t* to be added; must be NULL terminated.
 * @see brpc_list_add
 */
#define brpc_list(args...)	brpc_seq(BRPC_VAL_LIST, ##args)
/**
 * Builds a BINRPC map value.
 * @param args List of brpc_val_t* to be added; must be NULL terminated.
 * @see brpc_map_add
 */
#define brpc_map(args...)	brpc_seq(BRPC_VAL_MAP, ##args)
/**
 * Builds an empty AVP.
 * @see brpc_avp_add
 */
#define brpc_empty_avp()	brpc_seq(BRPC_VAL_AVP, NULL)

/*
 * Access records (ro).
 */

/**
 * Queries whether the BINRPC message is a failure signal.
 * @param brpc_val_t* whose type is queried.
 * @return Boolean true if failure signal.
 */
#define brpc_val_type(_ptr_)		(_ptr_)->type

/**
 * Queries whether the BINRPC message is a failure signal.
 * @param _ptr_ brpc_val_t* whose value is tested.
 * @return Boolean true if failure signal.
 */
#define brpc_is_null(_ptr_)		(_ptr_)->null

/**
 * Returns the value of a BINRCP int value.
 * @param _ptr_ brpc_val_t* to be interpreted as an integer.
 * @return The integer value.
 */
#define brpc_int_val(_ptr_)		(_ptr_)->val.int32

/**
 * Interpret a BINRPC value as bin.
 * @param _ptr_ brpc_val_t* to be interpreted as binary.
 * @eturn A brpc_bin_t structure.
 */
#define brpc_bin_val(_ptr_)		(_ptr_)->val.bin

/**
 * Interpret a BINRPC value as string.
 * @param _ptr_ brpc_val_t* to be interpreted as string.
 * @eturn A brpc_str_t structure.
 */
#define brpc_str_val(_ptr_)		(_ptr_)->val.str

/**
 * Interpret a BINRPC value as an AVP name.
 * @param _ptr_ brpc_val_t* to be interpreted as AVP name.
 * @return brpc_val_t* as AVP's name.
 */
#define brpc_avp_name(_ptr_)	_BRPC_VAL4LIST((_ptr_)->val.seq.list.next)

/**
 * Interpret a BINRPC value as an AVP value.
 * @param _ptr_ brpc_val_t* to be interpreted as AVP value.
 * @return brpc_val_t* as AVP's value.
 */
#define brpc_avp_val(_ptr_)		\
		_BRPC_VAL4LIST((_ptr_)->val.seq.list.next->next)

/**
 * Get the elements in a list.
 * @param x brpc_val_t* BINRPC list value.
 * @return brpc_val_t** array.
 */
#define brpc_list_elems(x)			brpc_seq_cells(x)

/**
 * Get the (top) elements in a map.
 * @param x brpc_val_t* BINRPC map value.
 * @return brpc_val_t** array.
 */
#define brpc_map_avps(x)			brpc_seq_cells(x)

/**
 * Mark a value as locked. Manually locking can be used to signal the library
 * to clone a value; this allows reusing the locked value.
 * @param _val_ brpc_val_t* value to lock.
 */
#define brpc_val_lock(_val_)		(_val_)->locked = true

/**
 * Retrieve the top-level values count in a sequence.
 * @param _val_ Reference to sequence.
 */
#define brpc_val_seqcnt(_val_)		((const size_t)(_val_)->val.seq.cnt)

/**
 * Misc functions.
 */

#define BRPC_STR_STATIC_INIT(name, _cstr_) \
		static brpc_str_t name = {_cstr_, sizeof(_cstr_)}
#define BRPC_STR_FMT(_str_) \
		(int)((_str_) ? (_str_)->len : (sizeof("(nil)") - /*0-term*/1)), \
		(_str_) ? (_str_)->val : "(nil)"
#define BRPC_STR_EQ(_a_, _b_) \
	(((_a_)->len == (_b_)->len) && \
	(strncmp((_a_)->val, (_b_)->val, (_b_)->len) == 0))


#endif /* __BRPC_VALUE_H__ */


