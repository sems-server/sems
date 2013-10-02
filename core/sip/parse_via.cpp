/*
 * $Id: parse_via.cpp 1713 2010-03-30 14:11:14Z rco $
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

#include "parse_via.h"
#include "parse_common.h"

#include "log.h"

#include <string>
using std::string;

#include <memory>
using std::auto_ptr;

#include "AmUtils.h"

sip_via_parm::sip_via_parm()
    : eop(NULL),params(),
      trans(), host(), 
      port(), port_i(),
      branch(), 
      recved(),
      has_rport(false),
      rport(),
      rport_i()
{
}

sip_via_parm::sip_via_parm(const sip_via_parm& p)
    : eop(NULL),params(),
      trans(p.trans), host(p.host), 
      port(p.port), port_i(p.port_i),
      branch(p.branch), 
      recved(p.recved),
      has_rport(p.has_rport),
      rport(p.rport),
      rport_i(p.rport_i)
{
}

sip_via_parm::~sip_via_parm()
{
    free_gen_params(&params);
}

sip_via::~sip_via()
{
    list<sip_via_parm*>::iterator it = parms.begin();
    for(;it != parms.end(); ++it) {

	delete *it;
    }
}

static int parse_transport(sip_transport* t, const char** c, int len)
{
    enum {

	TR_BEG,
	TR_UDP,
	TR_T,
	TR_TCP,
	TR_TLS,
	TR_SCTP,
	TR_OTHER
    };

    if(len < (int)SIPVER_len + 2){ // at least "SIP/2.0/?"
	DBG("Transport protocol is too small\n");
	return MALFORMED_SIP_MSG;
    }

    if (parse_sip_version(*c,SIPVER_len)) {
	DBG("Wrong/unsupported SIP version\n");
	return MALFORMED_SIP_MSG;
    }
    
    *c += SIPVER_len;
    
    if (*(*c)++ != '/') {
	DBG("Missing '/' after SIP version\n");
	return MALFORMED_SIP_MSG;
    }

    int st = TR_BEG;
    t->val.s = *c;

    len -= SIPVER_len+1;
    const char* end = *c + len;

    for(;**c && (*c!=end);(*c)++){

	switch(st){

	case TR_BEG:
	    switch(**c){

	    case 'u':
	    case 'U':
		if ( (len >= 3) &&
		     ( (*(*c+1) == 'd') || (*(*c+1) == 'D') ) &&
		     ( (*(*c+2) == 'p') || (*(*c+2) == 'P') ) ) {

		    t->type = sip_transport::UDP;
		    st = TR_UDP;
		    *c += 2;
		}
		else
		    st = TR_OTHER;
		break;

	    case 't':
	    case 'T':
		st = TR_T;
		break;

	    case 's':
	    case 'S':
		if ( (len >= 4) && 
		     ( (*(*c+1) == 'c') || (*(*c+1) == 'C') ) &&
		     ( (*(*c+2) == 't') || (*(*c+2) == 'T') ) &&
		     ( (*(*c+3) == 'p') || (*(*c+3) == 'P') ) ) {

		    t->type = sip_transport::SCTP;
		    st = TR_SCTP;
		    *c += 3;
		}
		else
		    st = TR_OTHER;
		break;
		
	    default:
		st = TR_OTHER;
		break;
	    }
	    break;

	case TR_T:
	    switch(**c){
	    case 'l':
	    case 'L':
		if( (len >= 3) && 
		    ( (*(*c+1) == 's') || (*(*c+1) == 'S')) ){
		    t->type = sip_transport::TLS;
		    st = TR_TLS;
		    (*c)++;
		}
		else
		    st = TR_OTHER;
		break;
		    
	    case 'c':
	    case 'C':
		if((len >= 3) &&
		    ( (*(*c+1) == 'p') || (*(*c+1) == 'P')) ){
		    t->type = sip_transport::TCP;
		    st = TR_TCP;
		    (*c)++;
		}
		else
		    st = TR_OTHER;
		break;

	    default:
		st = TR_OTHER;
		break;
	    }
	    break;

	case TR_UDP:
	case TR_TCP:
	case TR_TLS:
	case TR_SCTP:
	    switch(**c){
	    case '\0':
	    case CR:
	    case LF:
	    case SP:
		t->val.len = *c - t->val.s;
		return 0;

	    default:
		st = TR_OTHER;
		break;
	    }
	    break;

	case TR_OTHER:
	    switch(**c){
	    case '\0':
	    case CR:
	    case LF:
	    case SP:
		t->val.len = *c - t->val.s;
		t->type = sip_transport::OTHER;
		return 0;
	    }
	    break;
	}
    }

    t->val.len = *c - t->val.s;
    t->type = sip_transport::OTHER;
    return 0;
}

static int parse_by(sip_via_parm* v, const char** c, int len)
{
    enum {
	BY_HOST=0,
	BY_HOST_V6,
	BY_COLON,
	BY_PORT_SWS,
	BY_PORT
    };

    int saved_st=0, st=BY_HOST;

    const char* beg = *c;
    const char* end = beg+len;

    for(;*c!=end;(*c)++){

	switch(st){

	case BY_HOST:
	    switch(**c){

	    case_CR_LF;

	    case '[':
		st = BY_HOST_V6;
		break;

	    case ':':
		st = BY_PORT_SWS;
		v->host.set(beg,*c - beg);
		break;

	    case ';':
		goto end_by;

	    case SP:
	    case HTAB:
		st = BY_COLON;
		v->host.set(beg,*c - beg);
		break;
	    }
	    break;

	case BY_HOST_V6:
	    switch(**c){

	    case ']':
		st = BY_COLON;
		v->host.set(beg,*c+1 - beg);
		break;

	    case SP:
	    case HTAB:
	    case CR:
	    case LF:
		DBG("Bad character in IPv6 address (0x%x)\n",**c);
		return MALFORMED_SIP_MSG;
	    }
	    break;

	case BY_COLON:
	    switch(**c){

	    case_CR_LF;

	    case ':':
		st = BY_PORT_SWS;
		break;

	    case ';':
		goto end_by;

	    case SP:
	    case HTAB:
		break;

	    default:
		DBG("LWS expected\n");
		return MALFORMED_SIP_MSG;
	    }
	    break;

	case BY_PORT_SWS:

	    switch(**c){

	    case_CR_LF;

	    case SP:
	    case HTAB:
		break;

	    default:
		st = BY_PORT;
		beg = *c;
		v->port_i = 0;
		(*c)--;
		break;
	    }
	    break;

	case BY_PORT:

	    switch(**c){

	    case_CR_LF;

	    case SP:
	    case HTAB:
	    case ';':
		goto end_by;
	    }
	    if((**c < '0') ||(**c > '9')){
		DBG("bad character in port number (0x%x)\n",**c);
		return MALFORMED_SIP_MSG;
	    }
	    v->port_i = v->port_i*10 + (**c - '0'); 
	    break;

	case_ST_CR(**c);

	case ST_LF:
	case ST_CRLF:
	    switch(saved_st){
	    case BY_HOST:
		saved_st = BY_COLON;
		v->host.set(beg,*c - (st==ST_CRLF?2:1) - beg);
		break;
	    case BY_PORT:
		goto end_by;
	    }
	    st = saved_st;
	    break;
	}
    }

 end_by:
    switch(st){
    case BY_HOST:
	v->host.set(beg,*c-beg);
	break;
    case BY_PORT:
	v->port.set(beg,*c-beg);
	break;
    case ST_LF:
    case ST_CRLF:
	switch(saved_st){
	case BY_PORT:
	    v->port.set(beg,*c - (st==ST_CRLF?2:1) - beg);
	    break;
	}
	break;

    case BY_COLON:
	break;

    default:
	DBG("Unexpected end state: st = %i\n",st);
	return UNDEFINED_ERR;
    }

    //DBG("Via 'sent-by': '%.*s:%i'\n",v->host.len,v->host.s,v->port_i);

    return 0;
}

inline int parse_via_params(sip_via_parm* parm, const char** c, int len)
{
    enum {
	VP_BEG=0,

	VP_BRANCH1,
	VP_BRANCH2,
	VP_BRANCH3,
	VP_BRANCH4,
	VP_BRANCH5,
	VP_BRANCH,

	VP_R,

	VP_RECVD1,
	VP_RECVD2,
	VP_RECVD3,
	VP_RECVD4,
	VP_RECVD5,
	VP_RECVD6,
	VP_RECVD,

	VP_RPORT1,
	VP_RPORT2,
	VP_RPORT3,
	VP_RPORT,

	VP_OTHER
    };

    int ret = parse_gen_params_sc(&parm->params,c,len,',');
    if(ret) return ret;

    list<sip_avp*>::iterator it = parm->params.begin();
    for(;it != parm->params.end();++it){
	
	const char* c   = (*it)->name.s;
	const char* end = c + (*it)->name.len;
	int   st  = VP_BEG;

	for(;c!=end;c++){

	    switch(st){
		
	    case VP_BEG:
		switch(*c){
		case 'b':
		case 'B':
		    st = VP_BRANCH1;
		    break;

		case 'r':
		case 'R':
		    st = VP_R;
		    break;

		default:
		    st = VP_OTHER;
		    break;
		}
		break;

	    case VP_R:
		
		switch(*c){
		case 'e':// "re..."
		case 'E':
		    st = VP_RECVD1;
		    break;

		case 'p':// "rp..."
		case 'P':
		    st = VP_RPORT1;
		    break;

		default:
		    st = VP_OTHER;
		    break;
		}
		break;

#define case_VIA_PARAM(st1,ch1,ch2,st2)\
	    case st1:\
		switch(*c){\
		case ch1:\
		case ch2:\
		    st = st2;\
		    break;\
		default:\
		    st = VP_OTHER;\
		    break;\
		}\
		break

		case_VIA_PARAM(VP_BRANCH1,'r','R',VP_BRANCH2);
		case_VIA_PARAM(VP_BRANCH2,'a','A',VP_BRANCH3);
		case_VIA_PARAM(VP_BRANCH3,'n','N',VP_BRANCH4);
		case_VIA_PARAM(VP_BRANCH4,'c','C',VP_BRANCH5);
		case_VIA_PARAM(VP_BRANCH5,'h','H',VP_BRANCH);

		// "re"
		case_VIA_PARAM(VP_RECVD1,'c','C',VP_RECVD2);
		case_VIA_PARAM(VP_RECVD2,'e','E',VP_RECVD3);
		case_VIA_PARAM(VP_RECVD3,'i','I',VP_RECVD4);
		case_VIA_PARAM(VP_RECVD4,'v','V',VP_RECVD5);
		case_VIA_PARAM(VP_RECVD5,'e','E',VP_RECVD6);
		case_VIA_PARAM(VP_RECVD6,'d','D',VP_RECVD);

		// "rp"
		case_VIA_PARAM(VP_RPORT1, 'o','O',VP_RPORT2);
		case_VIA_PARAM(VP_RPORT2, 'r','R',VP_RPORT3);
		case_VIA_PARAM(VP_RPORT3, 't','T',VP_RPORT);

	    case VP_OTHER:
		goto next_param;
	    }
	}

	switch(st){
	case VP_BRANCH:
	    parm->branch = (*it)->value;
	    DBG("parsed branch: %.*s\n",(*it)->value.len,(*it)->value.s);
	    break;
	case VP_RECVD:
	    parm->recved = (*it)->value;
	    break;
	case VP_RPORT:
	    parm->has_rport = true;
	    parm->rport = (*it)->value;
	    if(parm->rport.len) {
		if(str2i(c2stlstr(parm->rport),parm->rport_i)) {
		    DBG("invalid port number in Via 'sent-by' address.\n");
		}
	    }
	    else {
		parm->rport.s = (*it)->name.s + (*it)->name.len;
	    }
	    break;
	}
	
    next_param:
	continue; // makes compiler happy
    }

    DBG("has_rport: %i\n",parm->has_rport);
    
    return ret;
}


int parse_via(sip_via* via, const char* beg, int len)
{
    enum {
	
	V_TRANS=0,
	V_URI,
	V_PARM_SEP,
	V_PARM_SEP_SWS
    };


    const char* c   = beg;
    const char* end = beg+len;

    int saved_st=0, st=V_TRANS;

    auto_ptr<sip_via_parm> parm(new sip_via_parm());

    int ret = 0;
    for(;c<end;c++){

 	switch(st){

	case V_TRANS:
 	    ret = parse_transport(&parm->trans, &c, end-c);
	    if(ret) return ret;

	    st = V_URI;
 	    break;
	    
 	case V_URI:
	    switch(*c){

	    case_CR_LF;
		
	    case SP:
	    case HTAB:
		break;

	    default:
		ret = parse_by(parm.get(), &c, end-c);
		if(ret) return ret;

		ret = parse_via_params(parm.get(),&c,end-c);
		if(ret) return ret;

		parm->eop = c;
		via->parms.push_back(parm.release());
		parm.reset(new sip_via_parm());
		
		st = V_PARM_SEP;
		c--;
		break;
	    }
	    break;

	case V_PARM_SEP:
	    switch(*c){

	    case_CR_LF;

	    case SP:
	    case HTAB:
		break;
		
	    case ',':
		st = V_PARM_SEP_SWS;
		break;

	    default:
		DBG("',' expected, found '%c'\n",*c);
		return MALFORMED_SIP_MSG;
	    }
	    break;

	case V_PARM_SEP_SWS:
	    switch(*c){

	    case_CR_LF;

	    case SP:
	    case HTAB:
		break;
		
	    default:
		st = V_TRANS;
		c--;
		break;
	    }
	    break;

	case_ST_CR(*c);

	case ST_LF:
	case ST_CRLF:
	    st = saved_st;
	    break;
	}
    }

    return 0;
}

/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
