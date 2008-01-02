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

#include <stdio.h>
#include <stdlib.h>

#include "log.h"
#include "misc.h"
#include "list.h"
#include "print.h"

__LOCAL ssize_t _brpc_val_print(const brpc_val_t *val, char *buff, size_t len);

__LOCAL ssize_t brpc_print_seq(const struct brpc_list_head *head, char *buff, 
		size_t len)
{
	brpc_val_t *val;
	ssize_t off = 0;
	brpc_seqval_list_for_each(val, head) {
		if (off)
			buff[off ++] = ',';
		if (off < len)
			off += _brpc_val_print(val, buff + off, len - off);
		else
			break;
	}
	return off;
}

__LOCAL ssize_t _brpc_val_print(const brpc_val_t *val, char *buff, size_t len)
{
	char start, end, *type;
	ssize_t off;
	int i;
	union {
		brpc_bin_t bin;
		brpc_str_t str;
	} cseq;

	if (brpc_is_null(val)) {
		switch (val->type) {
			case BRPC_VAL_LIST: type = "list"; break;
			case BRPC_VAL_AVP: type = "avp"; break;
			case BRPC_VAL_MAP: type = "map"; break;
			case BRPC_VAL_INT: type = "int"; break;
			case BRPC_VAL_FLOAT: type = "float"; break;
			case BRPC_VAL_STR: type = "str"; break;
			case BRPC_VAL_BIN: type = "bin"; break;
			
			case BRPC_VAL_RESERVED:
			case BRPC_VAL_NONE:
				BUG("unexpected descriptor type %d.\n", val->type);
				abort();
			default:
				BUG("unexpected value of type %d.\n", val->type);
				abort();
		}
		return snprintf(buff, len, "|nil:%s|", type);
	}

	switch (val->type) {
		do {
			case BRPC_VAL_LIST: start = '['; end = ']'; break;
			case BRPC_VAL_AVP: start = '<'; end = '>'; break;
			case BRPC_VAL_MAP: start = '{'; end = '}'; break;
		} while (0);
			off = 0;
			if (len)
				buff[off ++] = start;
			if (len) {
				off += brpc_print_seq(&val->val.seq.list, buff + off, 
						len - off);
				if (off < len) {
					buff[off ++] = end;
				}
			}
			return off;

		case BRPC_VAL_INT:
			return snprintf(buff, len, "%d", brpc_int_val(val));
		case BRPC_VAL_FLOAT:
			//TODO:
			//return snprintf(buff, len, "%lf", brpc_float_val(val));
			return snprintf(buff, len, "%d", brpc_int_val(val));
		case BRPC_VAL_STR:
			cseq.str = brpc_str_val(val);
			return snprintf(buff, len, "\"%.*s\"", BRPC_STR_FMT(&cseq.str));
		case BRPC_VAL_BIN:
			cseq.bin = brpc_bin_val(val);
			i = off = 0;
			while (i < cseq.bin.len) {
				if (off)
					buff[off ++] = ' ';
				if (off < len) {
					off += snprintf(buff + off, len - off, "0x%.2x ", 
							cseq.bin.val[i]);
				} else {
					break;
				}
			}
			return off;

		case BRPC_VAL_RESERVED:
		case BRPC_VAL_NONE:
			BUG("unexpected descriptor type %d.\n", val->type);
			abort();

		default:
			BUG("unknown descriptor type %d.\n", val->type);
			abort();
	}
}

ssize_t brpc_val_print(const brpc_val_t *val, char *buff, size_t len)
{
	ssize_t off;
#ifdef PARANOID
	if (INT_MAX < len) {
		ERR("buffer to large (%zd).\n", len);
		WERRNO(EINVAL);
		/* prevent snprintf from failing */
		return -1;
	}
#endif
	off =  _brpc_val_print(val, buff, len);
	if (off < len) {
		buff[off ++] = 0;
	} else {
		buff[off - 1] = 0;
	}
	return off;
}

ssize_t brpc_print(brpc_t *msg, char *buff, size_t len)
{
	const brpc_str_t *mname;
	brpc_val_t *val;
	struct brpc_list_head *head, *pos;
	ssize_t off = 0, hdr;

	if (msg->locked)
		if (! brpc_unpack(msg)) {
			ERR("failed to unpack message");
			return -1;
		}
	
	if (brpc_type(msg) == BRPC_CALL_REQUEST) {
		if (! (mname = brpc_method(msg))) {
			BUG("failed to retrieve request's method name.\n");
			abort();
		}
		off += snprintf(buff, len, "%.*s#%u(", BRPC_STR_FMT(mname), 
				brpc_id(msg));
		head = &msg->vals.list;
		pos = head->next->next;
	} else {
		off += snprintf(buff, len, "_#%u(", brpc_id(msg));
		head = msg->vals.list.next;
		pos = head->next;
	}

	buff += off;
	len -= off;
	hdr = off;
	off = 0;
	while ((off < len) && (head != pos)) {
		val = _BRPC_VAL4LIST(pos);
		if (off)
			if (off < len) {
				buff[off ++] = ',';
				len --;
			}
		off += _brpc_val_print(val, buff + off, len - off);
		pos = pos->next;
	}
	
	if (off < len)
		buff[off ++] = ')';

	if (off < len) {
		buff[off ++] = 0;
	} else {
		buff[off - 1] = 0;
	}

	return off + hdr;
}
