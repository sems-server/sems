/*
 * $Id$
 *
 * Copyright (C) 2007 Raphael Coeffic
 *
 * This file is part of sems, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * sems is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include "sip_parser.h"
#include "parse_header.h"
#include "parse_common.h"
#include "parse_via.h"
#include "parse_cseq.h"
#include "parse_from_to.h"

#include "log.h"

#include <memory>
using std::auto_ptr;

sip_msg::sip_msg(char* msg_buf, int msg_len)
    : buf(NULL),
      hdrs(),
      to(NULL),
      from(NULL),
      cseq(NULL),
      via1(NULL),via_p1(NULL),
      callid(NULL),
      contact(NULL),
      route(),
      record_route(),
      content_type(NULL),
      content_length(NULL),
      body()
{
    u.request = 0;
    u.reply   = 0;

    buf = new char[msg_len+1];
    memcpy(buf,msg_buf,msg_len);
    buf[msg_len] = '\0';
    len = msg_len;

    memset(&local_ip,0,sizeof(sockaddr_storage));
    memset(&remote_ip,0,sizeof(sockaddr_storage));
}

sip_msg::sip_msg()
    : buf(NULL),
      hdrs(),
      to(NULL),
      from(NULL),
      cseq(NULL),
      via1(NULL),via_p1(NULL),
      callid(NULL),
      contact(NULL),
      route(),
      record_route(),
      content_type(NULL),
      content_length(NULL),
      body()
{
    u.request = 0;
    u.reply   = 0;

    memset(&local_ip,0,sizeof(sockaddr_storage));
    memset(&remote_ip,0,sizeof(sockaddr_storage));
}

sip_msg::~sip_msg()
{
    DBG("~sip_msg()\n");

    delete [] buf;

    list<sip_header*>::iterator it;
    for(it = hdrs.begin();
	it != hdrs.end(); ++it) {

	//DBG("delete 0x%p\n",*it);
	delete *it;
    }

    if(u.request){
	if(type == SIP_REQUEST){
	    delete u.request;
	}
	else {
	    delete u.reply;
	}
    }
}


char* INVITEm = "INVITE";
#define INVITE_len 6

char* ACKm = "ACK";
#define ACK_len 3

char* OPTIONSm = "OPTIONS";
#define OPTIONS_len 7

char* BYEm = "BYE";
#define BYE_len 3

char* CANCELm = "CANCEL";
#define CANCEL_len 6

char* REGISTERm = "REGISTER";
#define REGISTER_len 8


int parse_method(int* method, char* beg, int len)
{
    char* c = beg;
    char* end = c+len;

    *method = sip_request::OTHER_METHOD;

    switch(len){
    case INVITE_len:
    //case CANCEL_len:
	switch(*c){
	case 'I':
	    if(!memcmp(c+1,INVITEm+1,INVITE_len-1)){
		//DBG("Found INVITE\n");
		*method = sip_request::INVITE;
	    }
	    break;
	case 'C':
	    if(!memcmp(c+1,CANCELm+1,CANCEL_len-1)){
		//DBG("Found CANCEL\n");
		*method = sip_request::CANCEL;
	    }
	    break;
	}
	break;

    case ACK_len:
    //case BYE_len:
	switch(*c){
	case 'A':
	    if(!memcmp(c+1,ACKm+1,ACK_len-1)){
		//DBG("Found ACK\n");
		*method = sip_request::ACK;
	    }
	    break;
	case 'B':
	    if(!memcmp(c+1,BYEm+1,BYE_len-1)){
		//DBG("Found BYE\n");
		*method = sip_request::BYE;
	    }
	    break;
	}

    case OPTIONS_len:
	if(!memcmp(c+1,OPTIONSm+1,OPTIONS_len-1)){
	    //DBG("Found OPTIONS\n");
	    *method = sip_request::OPTIONS;
	}
	break;

    case REGISTER_len:
	if(!memcmp(c+1,REGISTERm+1,REGISTER_len-1)){
	    //DBG("Found REGISTER\n");
	    *method = sip_request::REGISTER;
	}
	break;
    }
    
    // other method
    for(;c!=end;c++){
	if(!IS_TOKEN(*c)){
	    DBG("!IS_TOKEN(%c): MALFORMED_SIP_MSG\n",*c);
	    return MALFORMED_SIP_MSG;
	}
    }

    if(*method == sip_request::OTHER_METHOD){
	//DBG("Found other method (%.*s)\n",len,beg);
    }

    return 0;
}


static int parse_first_line(sip_msg* msg, char** c)
{
    enum {
	FL_METH=0,
	FL_RURI,
	
	FL_SIPVER1,     // 'S'
	FL_SIPVER2,     // 'I'
	FL_SIPVER3,     // 'P'
	FL_SIPVER4,     // '/'
	FL_SIPVER_DIG1, // 1st digit
	FL_SIPVER_SEP,  // '.'
	FL_SIPVER_DIG2, // 2st digit

	FL_SIPVER_SP,   // ' '

	FL_STAT1,
	FL_STAT2,
	FL_STAT3,
	FL_STAT_SP,
	FL_REASON,

	FL_EOL,
	FL_ERR
    };

    char* beg = *c;
    int saved_st=0, st=FL_SIPVER1;
    int err=0;

    bool is_request=false;

    for(;**c;(*c)++){

	switch(st){

#define case_SIPVER(ch,st1,st2,else_st) \
	case st1:\
	    if(**c == (ch)){\
		st = st2;\
	    }\
	    else {\
		st = else_st;\
	    }\
	    break
            
	case_SIPVER('S',FL_SIPVER1,FL_SIPVER2,FL_METH;(*c)--);
	case_SIPVER('I',FL_SIPVER2,FL_SIPVER3,FL_METH;(*c)--);
	case_SIPVER('P',FL_SIPVER3,FL_SIPVER4,FL_METH;(*c)--);

	case FL_SIPVER4:     // '/'
	    if(**c == '/'){
		st = FL_SIPVER_DIG1;
	    }
	    else if(!is_request){
		st = FL_METH;
		(*c)--;
	    }
	    else {
		st = FL_ERR;
	    }
	    break;

	case_SIPVER('2',FL_SIPVER_DIG1,FL_SIPVER_SEP,FL_ERR);
	case_SIPVER('.',FL_SIPVER_SEP,FL_SIPVER_DIG2,FL_ERR);

#undef case_SIPVER

	case FL_SIPVER_DIG2:
	    if(**c == '0'){
		if(is_request)
		    st = FL_EOL;
		else {
		    msg->type = SIP_REPLY;
		    msg->u.reply = new sip_reply;
		    st = FL_SIPVER_SP;
		}
	    }
	    break;

	case FL_METH:
	    switch(**c){
	    case SP:
		msg->type = SIP_REQUEST;
		msg->u.request = new sip_request;
		msg->u.request->method_str.set(beg,*c-beg);
		err = parse_method(&msg->u.request->method,beg,*c-beg);
		if(err)
		    return err;
		st = FL_RURI;
		beg = *c+1;
		break;

// 	    default:
// 		if(!IS_TOKEN(**c)){
// 		    DBG("Bad char in request method: 0x%.2x\n",**c);
// 		    return MALFORMED_SIP_MSG;
// 		}
// 		break;
	    }
	    break;
	    
	case FL_RURI:
	    switch(**c){
	    case SP:
		msg->u.request->ruri_str.set(beg, *c-beg); 
		err = parse_uri(&msg->u.request->ruri, beg, *c-beg);
		if(err)
		    return err;
		st = FL_SIPVER1;
		is_request = true;
		break;

	    case CR:
	    case LF:
	    case HTAB:
		DBG("Bad char in request URI: 0x%x\n",**c);
		return MALFORMED_SIP_MSG;
	    }
	    break;

	case FL_SIPVER_SP:
	    if(**c != SP) st = FL_ERR;
	    else {
		st = FL_STAT1;
		beg = *c+1;
	    }
	    break;

#define case_STCODE(st1) \
	case st1:\
	    if(IS_DIGIT(**c)){\
		st++;\
		msg->u.reply->code *= 10;\
		msg->u.reply->code += **c - '0';\
	    }\
	    else {\
		st = FL_ERR;\
	    }\
	    break

	case_STCODE(FL_STAT1);
	case_STCODE(FL_STAT2);
	case_STCODE(FL_STAT3);

#undef case_STCODE
	    
	case FL_STAT_SP:
	    if(**c != SP) st = FL_ERR;
	    else {
		st = FL_REASON;
		beg = *c+1;
	    }
	    break;

	case FL_REASON:
	    switch(**c){
	    case_CR_LF;
	    }
	    break;

	case FL_EOL:
	    switch(**c){
	    case_CR_LF;
	    default:
		DBG("Bad char at the end of first line: 0x%x\n",**c);
		return MALFORMED_SIP_MSG;
	    }
	    break;

	case FL_ERR:
	    return MALFORMED_SIP_MSG;

	case_ST_CR(**c);

	case ST_LF:
	case ST_CRLF:
	    if(saved_st == FL_REASON){
		msg->u.reply->reason.set(beg,*c-(st==ST_CRLF?2:1)-beg);
	    }
	    return 0;

	default:
	    DBG("Bad state! st=%i\n",st);
	    return -99;
	}
    }

    return UNEXPECTED_EOT;
}


int parse_sip_msg(sip_msg* msg)
{
    char* c = msg->buf;

    int err_fl = parse_first_line(msg,&c);

    if(err_fl == UNEXPECTED_EOT)
	return err_fl;

    int err_hdrs = parse_headers(msg,&c);

    if(!msg->via1){
	DBG("Missing via header\n");
	return MALFORMED_SIP_MSG;
    }

    auto_ptr<sip_via> via(new sip_via());
    if(!parse_via(via.get(), 
		  msg->via1->value.s,
		  msg->via1->value.len) && 
       !via->parms.empty() ) {

	msg->via_p1 = *via->parms.begin();
	msg->via1->p = via.release();

// 	DBG("first via: proto=<%i>, addr=<%.*s:%.*s>, branch=<%.*s>\n", 
// 	    msg->via_p1->trans.type, 
// 	    msg->via_p1->host.len,
// 	    msg->via_p1->host.s,
// 	    (msg->via_p1->port.len ? msg->via_p1->port.len : 4),
// 	    (msg->via_p1->port.len ? msg->via_p1->port.s : "5060"),
// 	    msg->via_p1->branch.len,
// 	    msg->via_p1->branch.s );

    }
    else
	return MALFORMED_SIP_MSG;


    if(!msg->cseq){
	DBG("Missing cseq header\n");
	return MALFORMED_SIP_MSG;
    }

    auto_ptr<sip_cseq> cseq(new sip_cseq());
    if(!parse_cseq(cseq.get(),
		   msg->cseq->value.s,
		   msg->cseq->value.len) &&
       cseq->str.len &&
       cseq->method.len ) {
	
// 	DBG("Cseq header: '%.*s' '%.*s'\n",
// 	    cseq->str.len,cseq->str.s,
// 	    cseq->method.len,cseq->method.s);

	msg->cseq->p = cseq.release();
    }
    else
	return MALFORMED_SIP_MSG;


    if(!msg->from){
	DBG("Missing from header\n");
	return MALFORMED_SIP_MSG;
    }

    auto_ptr<sip_from_to> from(new sip_from_to());
    if(!parse_from_to(from.get(),
		      msg->from->value.s,
		      msg->from->value.len)) {

// 	DBG("From header: name-addr=\"%.*s <%.*s>\"\n",
// 	    from->nameaddr.name.len,from->nameaddr.name.s,
// 	    from->nameaddr.addr.len,from->nameaddr.addr.s);

	msg->from->p = from.release();
    }
    else
	return MALFORMED_SIP_MSG;

    if(!msg->to){
	DBG("Missing to header\n");
	return MALFORMED_SIP_MSG;
    }

    auto_ptr<sip_from_to> to(new sip_from_to());
    if(!parse_from_to(to.get(),
		      msg->to->value.s,
		      msg->to->value.len)) {

// 	DBG("To header: name-addr=\"%.*s <%.*s>\"\n",
// 	    to->nameaddr.name.len,to->nameaddr.name.s,
// 	    to->nameaddr.addr.len,to->nameaddr.addr.s);

	msg->to->p = to.release();
    }
    else
	return MALFORMED_SIP_MSG;

    if(!(err_fl || err_hdrs)){
	msg->body.set(c,msg->len - (c - msg->buf));
    }

    return err_fl || err_hdrs;
}
