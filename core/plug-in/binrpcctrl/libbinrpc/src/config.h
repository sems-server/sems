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


#ifndef __BRPC_CONFIG_H__
#define __BRPC_CONFIG_H__

/* maximum supported size of a packet */
#define BINRPC_MAX_PKT_LEN	(1<<14)
/* 'standard' BINRPC port */
#define BINRPC_PORT			2046


#ifdef _LIBBINRPC_BUILD

/* (initial) lenght for buffer representation */
#define BINRPC_MAX_REPR_LEN	256

/* dissector's initial stack size */
#define BINRPC_DISSECTOR_SSZ	8

#ifndef BINRPC_LIB_VER
#define BINRPC_LIB_VER "?.?.?"
#warning "unknown version numbers"
#endif /* BINRPC_LIB_VER */

#endif /* _LIBBINRPC_BUILD */


#endif /* __BRPC_CONFIG_H__ */
