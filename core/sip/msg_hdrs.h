/*
 * $Id: msg_hdrs.h 1713 2010-03-30 14:11:14Z rco $
 *
 * Copyright (C) 2007 Raphael Coeffic
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. This program is released under
 * the GPL with the additional exemption that compiling, linking,
 * and/or using OpenSSL is allowed.
 *
 * For a license to use the SEMS software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * SEMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _msg_hdrs_h
#define _msg_hdrs_h

#include "parse_header.h"
#include "parse_common.h"
#include "parse_via.h"

inline int copy_hdr_len(sip_header* hdr)
{
    return hdr->name.len + hdr->value.len
	+ 4/* ': ' + CRLF */;
}

inline void copy_hdr_wr(char** c, const sip_header* hdr)
{
    memcpy(*c,hdr->name.s,hdr->name.len);
    *c += hdr->name.len;
    
    *((*c)++) = ':';
    *((*c)++) = SP;
    
    memcpy(*c,hdr->value.s,hdr->value.len);
    *c += hdr->value.len;
    
    *((*c)++) = CR;
    *((*c)++) = LF;
}

inline int contact_len(const cstring& contact)
{
    return 11/*'Contact: ' + CRLF*/
	+ contact.len;
}

inline void contact_wr(char** c,const cstring& contact)
{
    memcpy(*c,"Contact: ",9);
    *c += 9/*'Contact: '*/;
    
    memcpy(*c,contact.s,contact.len);
    *c += contact.len;
    
    *((*c)++) = CR;
    *((*c)++) = LF;
}

inline int via_len(const cstring& addr, const cstring& branch, bool rport)
{
    return 19/*'Via: SIP/2.0/UDP ' + CRLF*/
	+ addr.len
	+ 8 + MAGIC_BRANCH_LEN/*';branch=' + MAGIC_BRANCH_COOKIE*/
	+ branch.len
        + (rport ? 6/*;rport*/ : 0 );
}

inline void via_wr(char** c, const cstring& addr, const cstring& branch, bool rport)
{
    memcpy(*c,"Via: SIP/2.0/UDP ",17);
    *c += 17/*'Via: SIP/2.0/UDP '*/;
    
    memcpy(*c,addr.s,addr.len);
    *c += addr.len;

    memcpy(*c,";branch=" MAGIC_BRANCH_COOKIE,8+MAGIC_BRANCH_LEN);
    *c += 8+MAGIC_BRANCH_LEN;

    memcpy(*c,branch.s,branch.len);
    *c += branch.len;
    
    if(rport){
      memcpy(*c,";rport",6);
      *c += 6;
    }

    *((*c)++) = CR;
    *((*c)++) = LF;
}

inline int cseq_len(const cstring& num, const cstring& method)
{
    return 9/*'CSeq: ' + SP + CRLF*/
	+ num.len + method.len;
}

inline void cseq_wr(char** c, const cstring& num, const cstring& method)
{
    memcpy(*c,"CSeq: ",6);
    *c += 6/*'CSeq: '*/;

    memcpy(*c,num.s,num.len);
    *c += num.len;

    *((*c)++) = SP;

    memcpy(*c,method.s,method.len);
    *c += method.len;
    
    *((*c)++) = CR;
    *((*c)++) = LF;
}

inline int content_length_len(const cstring& len)
{
    return 18/*'Content-Length: ' + CRLF*/
	+ len.len;
}

inline void content_length_wr(char** c, const cstring& len)
{
    memcpy(*c,"Content-Length: ",16);
    *c += 16/*'Content-Length: '*/;

    memcpy(*c,len.s,len.len);
    *c += len.len;

    *((*c)++) = CR;
    *((*c)++) = LF;
}

inline int content_type_len(const cstring& len)
{
    return 16/*'Content-Type: ' + CRLF*/
	+ len.len;
}

inline void content_type_wr(char** c, const cstring& len)
{
    memcpy(*c,"Content-Type: ",14);
    *c += 14/*'Content-Type: '*/;

    memcpy(*c,len.s,len.len);
    *c += len.len;

    *((*c)++) = CR;
    *((*c)++) = LF;
}

#include <list>
using std::list;


int  copy_hdrs_len(const list<sip_header*>& hdrs);
void copy_hdrs_wr(char** c, const list<sip_header*>& hdrs);
void copy_hdrs_wr_no_via(char** c, const list<sip_header*>& hdrs);


#endif
