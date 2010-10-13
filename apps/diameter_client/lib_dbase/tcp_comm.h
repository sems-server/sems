/*
 * Digest Authentication - Diameter support
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2008-2009 iptego GmbH
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 * 
 * For a license to use the SEMS software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * -------
 *  
 *  
 */

#ifndef TCP_COMM_H
#define TCP_COMM_H

#include "defs.h"
#include "diameter_msg.h"

#include "sys/time.h"

#ifdef WITH_OPENSSL
#include <openssl/ssl.h>
#endif

#define MAX_WAIT_SEC	2
#define MAX_WAIT_USEC	0

#define MAX_AAA_MSG_SIZE  65536

#define CONN_SUCCESS	 1 
#define CONN_ERROR	-1
#define CONN_CLOSED	-2


#ifdef  __cplusplus
extern "C" {
#endif

#ifdef WITH_OPENSSL
  extern BIO* bio_err;
#endif

  struct dia_tcp_conn_t {
    int sockfd;
#ifdef WITH_OPENSSL
    SSL_CTX* ctx;
    SSL* ssl;
    BIO* sbio;
#endif
  };

  typedef  struct dia_tcp_conn_t dia_tcp_conn;

  /* initializes the lib/module */ 
  int tcp_init_tcp();

  /* initializes the TCP connection */ 
  dia_tcp_conn* tcp_create_connection(const char* host, int port,
				      const char* CA_file, const char* client_cert_file);

  /* send a message over an already opened TCP connection */
  int tcp_send(dia_tcp_conn* conn_st, char* buf, int len);

  /* receive reply
   */
  int tcp_recv_msg(dia_tcp_conn* conn_st, rd_buf_t* rb,  
		     time_t wait_sec, suseconds_t wait_usec);

  void tcp_close_connection(dia_tcp_conn* conn_st);

  void tcp_destroy_connection(dia_tcp_conn* conn_st);

  void tcp_tls_shutdown(dia_tcp_conn* conn_st);
#ifdef  __cplusplus
}
#endif

#endif
