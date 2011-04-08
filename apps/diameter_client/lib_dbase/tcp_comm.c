/*
 * Digest Authentication - Diameter support
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2007-2009 iptego GmbH
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <errno.h>
#include <string.h>

/* memory management */
#include "mem.h"

/* printing messages, dealing with strings and other utils */
#include "log.h"
#include "str.h"

/* headers defined by this module */
#include "defs.h"
#include "tcp_comm.h"
#include "avp.h"

#define MAX_TRIES	10

#define WANT_RW_TIMEOUT_USEC 100000 // 100 ms

void reset_read_buffer(rd_buf_t *rb);
int do_read(dia_tcp_conn* conn_st, rd_buf_t *p);

#ifdef WITH_OPENSSL
/* for printing error msg */
BIO* bio_err = 0;

long tcp_ssl_dbg_cb(BIO *bio, int oper, const char *argp,
		    int argi, long argl, long retvalue) {

  if (oper & BIO_CB_RETURN)
    return retvalue;

  switch (oper) {
  case BIO_CB_WRITE: {
    char buf[256];
    snprintf(buf, 256, "%s: %s", argp, bio->method->name);
    INFO("%s", buf);
  } break;

  case BIO_CB_PUTS: {
    char buf[2];
    buf[0] = *argp;
    buf[1] = '\0';
    INFO("%s", buf);
  } break;
  default: break;
  }

  return retvalue;
}

static int password_cb(char *buf,int num,
		       int rwflag,void *userdata) {
  ERROR("password protected key file.\n"); /* todo? */
  return 0;
}

/* Check that the common name matches the
   host name*/
int check_cert(SSL * ssl, char* host) {
  X509 *peer;
  char peer_CN[256];
  
  if(SSL_get_verify_result(ssl)!=X509_V_OK)  {
    ERROR("Certificate doesn't verify");
    return -1;
  }
  
  /*Check the cert chain. The chain length
    is automatically checked by OpenSSL when
    we set the verify depth in the ctx */
  
  /*Check the common name*/
  peer=SSL_get_peer_certificate(ssl);
  X509_NAME_get_text_by_NID
    (X509_get_subject_name(peer),
     NID_commonName, peer_CN, 256);
  if(strcasecmp(peer_CN,host)) {
    ERROR("Common name doesn't match host name");
    return -1;
  }

  return 0;
}

#endif

int tcp_init_tcp() {
#ifdef WITH_OPENSSL
  SSL_library_init();
  SSL_load_error_strings();
  bio_err = BIO_new(BIO_s_null());
  BIO_set_callback(bio_err, tcp_ssl_dbg_cb);
#endif
  return 0;
}

/* it initializes the TCP connection */ 
dia_tcp_conn* tcp_create_connection(const char* host, int port,
				    const char* CA_file, const char* client_cert_file)
{
  int sockfd;
  struct sockaddr_in serv_addr;
  struct hostent *server;
#ifdef WITH_OPENSSL
  SSL_METHOD* meth;
#endif
    
  sockfd = socket(PF_INET, SOCK_STREAM, 0);
	
  DBG("got DIAMETER socket #%d\n", sockfd); 

  if (sockfd < 0) 
    {
      ERROR(M_NAME":init_diatcp(): error creating the socket\n");
      return 0;
    }	
	
  server = gethostbyname(host);
  if (server == NULL) 
    {
      close(sockfd);
      ERROR( M_NAME":init_diatcp(): error finding the host '%s'\n",
	     host);
      return 0;
    }

  memset((char *) &serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = PF_INET;
  memcpy((char *)&serv_addr.sin_addr.s_addr, (char *)server->h_addr,
	 server->h_length);
  serv_addr.sin_port = htons(port);
	
  if (connect(sockfd, (const struct sockaddr *)&serv_addr, 
	      sizeof(serv_addr)) < 0) 
    {
      close(sockfd);
      ERROR( M_NAME":init_diatcp(): error connecting to the "
	     "DIAMETER peer '%s'\n", host);
      return 0;
    }	

  dia_tcp_conn* conn_st = pkg_malloc(sizeof(dia_tcp_conn));
  memset(conn_st, 0, sizeof(dia_tcp_conn));

  conn_st->sockfd = sockfd;

#ifdef WITH_OPENSSL
  if (!strlen(CA_file)) {
    DBG("no CA certificate - not using TLS.\n");
    return conn_st;
  }

  meth=(SSL_METHOD *)TLSv1_client_method();
  conn_st->ctx = SSL_CTX_new(meth);

  if (!conn_st->ctx) {
    ERROR("SSL: creating TLSv1_client_method context\n");
    tcp_close_connection(conn_st);
    return 0;
  }

  if (SSL_CTX_set_default_verify_paths(conn_st->ctx) != 1) {
    ERROR("SSL: SSL_CTX_set_default_verify_paths\n");
    SSL_CTX_free(conn_st->ctx);
    tcp_close_connection(conn_st);
    return 0;
  }

  if (!strlen(client_cert_file)) {
    DBG("no client certificate - not authenticating client.\n");
  } else {

    if (!SSL_CTX_use_certificate_chain_file(conn_st->ctx, client_cert_file)) {
      ERROR("using certificate from file '%s'\n",
	    client_cert_file);
      SSL_CTX_free(conn_st->ctx);
      tcp_close_connection(conn_st);
      pkg_free(conn_st);
      return 0;
    }
  
    SSL_CTX_set_default_passwd_cb(conn_st->ctx, password_cb);

    if(!(SSL_CTX_use_PrivateKey_file(conn_st->ctx,
				     client_cert_file,SSL_FILETYPE_PEM))) {
      ERROR("Loading private key file '%s'\n",
	    client_cert_file);
      SSL_CTX_free(conn_st->ctx);
      tcp_close_connection(conn_st);
      pkg_free(conn_st);
      return 0;
    }
  }
  
  /* Load the CAs we trust*/
  if(!(SSL_CTX_load_verify_locations(conn_st->ctx,
				     CA_file,0))) {
    ERROR("Loading CA file '%s'\n",
	  CA_file);
    SSL_CTX_free(conn_st->ctx);
    tcp_close_connection(conn_st);
    pkg_free(conn_st);
    return 0;
  }
  
#if (OPENSSL_VERSION_NUMBER < 0x00905100L)
  SSL_CTX_set_verify_depth(ctx,1);
#endif

  conn_st->ssl=SSL_new(conn_st->ctx);
  conn_st->sbio=BIO_new_socket(sockfd,BIO_NOCLOSE);
  SSL_set_bio(conn_st->ssl,conn_st->sbio,conn_st->sbio);
  if(SSL_connect(conn_st->ssl)<=0) {
    ERROR("in SSL connect\n");
    SSL_free(conn_st->ssl);
    SSL_CTX_free(conn_st->ctx);
    tcp_close_connection(conn_st);
    pkg_free(conn_st);
    return 0;
  }

#endif
/*   check_cert(ssl,host); */

  return conn_st;
}

void tcp_tls_shutdown(dia_tcp_conn* conn_st) {
#ifdef WITH_OPENSSL
  if (conn_st->ctx && conn_st->ssl) {
    SSL_shutdown(conn_st->ssl);
  }
#endif
}


void reset_read_buffer(rd_buf_t *rb)
{
  rb->ret_code		= 0;
  rb->chall_len		= 0;
  if(rb->chall)
    pkg_free(rb->chall);
  rb->chall		= 0;

  rb->first_4bytes	= 0;
  rb->buf_len		= 0;
  if(rb->buf)
    pkg_free(rb->buf);
  rb->buf		= 0;
}

int tryreceive(dia_tcp_conn* conn_st, unsigned char* ptr, int nwanted) {
#ifdef WITH_OPENSSL
  int res;
  fd_set rw_fd_set;
  struct timeval tv;

  tv.tv_sec = 0;
  tv.tv_usec = WANT_RW_TIMEOUT_USEC;
      
  if (conn_st->ssl) {
    while (1) {
      res = SSL_read(conn_st->ssl, ptr, nwanted);
      switch(SSL_get_error(conn_st->ssl,res)){
      case SSL_ERROR_NONE: {
	return res;
      }
	
      case SSL_ERROR_ZERO_RETURN: /* shutdown */
	DBG("SSL shutdown connection (in SSL_read)\n");
	return 0;
	
      case SSL_ERROR_WANT_READ: {
	FD_ZERO(&rw_fd_set);
	FD_SET(conn_st->sockfd, &rw_fd_set);
	res = select (conn_st->sockfd+1, &rw_fd_set, NULL, NULL, &tv);
	if ( res < 0) {
	  ERROR( M_NAME":SSL_WANT_READ select failed\n");
	  return -1;
	}
      } break;
	
      case SSL_ERROR_WANT_WRITE:  {
	FD_ZERO(&rw_fd_set);
	FD_SET(conn_st->sockfd, &rw_fd_set);
	res = select (conn_st->sockfd+1, NULL, &rw_fd_set, NULL, &tv);
	if ( res < 0) {
	  ERROR( M_NAME":SSL_WANT_WRITE select failed\n");
	  return -1;
	}
      } break;
      default: return 0;
      }
    }    
  } else {
#endif
    return recv(conn_st->sockfd, ptr, nwanted, MSG_DONTWAIT);
#ifdef WITH_OPENSSL
  }
#endif
}

/* read from a socket, an AAA message buffer */
int do_read(dia_tcp_conn* conn_st, rd_buf_t *p)
{
  unsigned char  *ptr;
  unsigned int   wanted_len, len;
  int n;

  if (p->buf==0)
    {
      wanted_len = sizeof(p->first_4bytes) - p->buf_len;
      ptr = ((unsigned char*)&(p->first_4bytes)) + p->buf_len;
    }
  else
    {
      wanted_len = p->first_4bytes - p->buf_len;
      ptr = p->buf + p->buf_len;
    }

  while( (n=tryreceive(conn_st, ptr, wanted_len))>0 ) 
    {
      //		DBG("DEBUG:do_read (sock=%d)  -> n=%d (expected=%d)\n",
      //			p->sock,n,wanted_len);
      p->buf_len += n;
      if (n<wanted_len)
	{
	  //DBG("only %d bytes read from %d expected\n",n,wanted_len);
	  wanted_len -= n;
	  ptr += n;
	}
      else 
	{
	  if (p->buf==0)
	    {
	      /* I just finished reading the the first 4 bytes from msg */
	      len = ntohl(p->first_4bytes)&0x00ffffff;
	      if (len<AAA_MSG_HDR_SIZE || len>MAX_AAA_MSG_SIZE)
		{
		  ERROR("ERROR:do_read (sock=%d): invalid message "
		      "length read %u (%x)\n", conn_st->sockfd, len, p->first_4bytes);
		  goto error;
		}
	      //DBG("message length = %d(%x)\n",len,len);
	      if ( (p->buf=pkg_malloc(len))==0  )
		{
		  ERROR("ERROR:do_read: no more free memory\n");
		  goto error;
		}
	      *((unsigned int*)p->buf) = p->first_4bytes;
	      p->buf_len = sizeof(p->first_4bytes);
	      p->first_4bytes = len;
	      /* update the reading position and len */
	      ptr = p->buf + p->buf_len;
	      wanted_len = p->first_4bytes - p->buf_len;
	    }
	  else
	    {
	      /* I finished reading the whole message */
#ifdef EXTRA_DEBUG 
	      DBG("DEBUG:do_read (sock=%d): whole message read (len=%d)!\n",
		  socket, p->first_4bytes);
#endif

	      return CONN_SUCCESS;
	    }
	}
    }

  if (n==0)
    {
      INFO("INFO:do_read (sock=%d): FIN received\n", conn_st->sockfd);
      return CONN_CLOSED;
    }
  if ( n==-1 && errno!=EINTR && errno!=EAGAIN )
    {
      ERROR("ERROR:do_read (sock=%d): n=%d , errno=%d (%s)\n",
	  conn_st->sockfd, n, errno, strerror(errno));
      goto error;
    }
 error:
  return CONN_ERROR;
}

int tcp_send(dia_tcp_conn* conn_st, char* buf, int len) {
  int n;
  int sockfd;
  fd_set rw_fd_set;
  struct timeval tv;

  tv.tv_sec = 0;
  tv.tv_usec = WANT_RW_TIMEOUT_USEC;


  if (!conn_st) {
    ERROR("called without conn_st\n");
    return CONN_ERROR;
  }
  
  sockfd = conn_st->sockfd;

#ifdef WITH_OPENSSL
  if (!conn_st->ssl) { 
#endif
  /* try to write the message to the Diameter client */
  while( (n=write(sockfd, buf, len))==-1 ) {
    if (errno==EINTR)
      continue;
    ERROR( M_NAME": write returned error: %s\n", strerror(errno));
    return AAA_ERROR;
  }
  
  if (n!=len) {
    ERROR( M_NAME": write gave no error but wrote less than asked\n");
    return AAA_ERROR;
  }
#ifdef WITH_OPENSSL
  } else {
    while (1) {
      n = SSL_write(conn_st->ssl, buf, len);
      switch(SSL_get_error(conn_st->ssl, n)) {
      case SSL_ERROR_NONE: {
	if (len != n) {
	  ERROR( M_NAME": write gave no error but wrote less than asked\n");
	  return AAA_ERROR;
	}
	return 0;
      };
      case SSL_ERROR_ZERO_RETURN: /* shutdown */
	DBG("SSL shutdown connection (in SSL_write)\n");
	return 0;
	
      case SSL_ERROR_WANT_READ: {
	FD_ZERO(&rw_fd_set);
	FD_SET(conn_st->sockfd, &rw_fd_set);
	n=select(conn_st->sockfd+1, &rw_fd_set, NULL, NULL, &tv);
	if (n < 0) {
	  ERROR( M_NAME":SSL_WANT_READ select failed\n");
	  return -1;
	}
      } break; /* try again */
	
      case SSL_ERROR_WANT_WRITE:  {
	FD_ZERO(&rw_fd_set);
	FD_SET(conn_st->sockfd, &rw_fd_set);
	n=select(conn_st->sockfd+1, NULL, &rw_fd_set, NULL, &tv);
	if (n < 0) {
	  ERROR( M_NAME":SSL_WANT_WRITE select failed\n");
	  return -1;
	}
      } break; /* try again */

      default: {
	ERROR("SSL write error.\n");
	return AAA_ERROR;
      }
      }
    }
  }
#endif

  return 0;
}

int tcp_recv_msg(dia_tcp_conn* conn_st, rd_buf_t* rb, 
		 time_t wait_sec, suseconds_t wait_usec) {
  int res;
  fd_set rd_fd_set;
  struct timeval tv;
  int sockfd;

  if (!conn_st) {
    ERROR("called without conn_st\n");
    return CONN_ERROR;
  }
  
  sockfd = conn_st->sockfd;

  /* wait for the answer a limited amount of time */
  tv.tv_sec = wait_sec;
  tv.tv_usec = wait_usec;

  /* Initialize the set of active sockets. */
  FD_ZERO (&rd_fd_set);
  FD_SET (sockfd, &rd_fd_set);

  res = select (sockfd+1, &rd_fd_set, NULL, NULL, &tv);
  if ( res < 0) {
    ERROR( M_NAME":tcp_reply_recv(): select function failed\n");
    return AAA_ERROR;
  }

  if (res == 0)
    return 0;

  /* Data arriving on a already-connected socket. */
  reset_read_buffer(rb);
  switch( do_read(conn_st, rb) )
    {
    case CONN_ERROR:
      ERROR( M_NAME":tcp_reply_recv(): error when trying to read from socket\n");
      return AAA_CONN_CLOSED;
    case CONN_CLOSED:
      INFO( M_NAME":tcp_reply_recv(): connection closed by diameter peer\n");
      return AAA_CONN_SHUTDOWN;
    }
  return 1; //received something
}

void tcp_close_connection(dia_tcp_conn* conn_st)
{
  if (!conn_st) {
    ERROR("called without conn_st\n");
    return;
  }
  
  shutdown(conn_st->sockfd, SHUT_RDWR);
  DBG("closing DIAMETER socket %d\n", conn_st->sockfd);
  close(conn_st->sockfd);
}

void tcp_destroy_connection(dia_tcp_conn* conn_st) {
  if (!conn_st) {
    ERROR("called without conn_st\n");
    return;
  }    

  if (conn_st->ssl)
    SSL_free(conn_st->ssl);
  if (conn_st->ctx)
    SSL_CTX_free(conn_st->ctx);

  pkg_free(conn_st);
}
