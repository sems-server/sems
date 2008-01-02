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

#ifndef __BRPC_ERRCODE_H__
#define __BRPC_ERRCODE_H__

/**
 * Possible types of BINRPC faults.
 */
enum BINRPC_ERROR_CODES {
	BRPC_ESRV = 1,	/**< internal server error */
	BRPC_EVER,		/**< version not supported  */
	BRPC_EMSG,		/**< bad message */
	BRPC_EMETH,		/**< no such call method*/
	BRPC_ESIGN,		/**< call has wrong signature */
	BRPC_EPARAM,	/**< wrong type of parameter */
	BRPC_ESIZE,		/**< answer too big */
	BRPC_EACK,		/**< received, but no answer can be issued */

	BRPC_E_MAX		/**< max val */
};

#endif /* __BRPC_ERRCODE_H__ */
