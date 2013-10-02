/*
 * $Id: parse_common.cpp 850 2008-04-04 21:29:36Z sayer $
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


#include "parse_common.h"
#include "log.h"

#include <string.h>

#include <memory>
using std::auto_ptr;

int parse_sip_version(const char* beg, int len)
{
    const char* c = beg;
    //char* end = c+len;

    if(len!=SIPVER_len){
	DBG("SIP-Version string length != SIPVER_len\n");
	return MALFORMED_SIP_MSG;
    }

    if( ((c[0] != 'S')&&(c[0] != 's')) || 
	((c[1] != 'I')&&(c[1] != 'i')) ||
	((c[2] != 'P')&&(c[2] != 'p')) ) {

	DBG("SIP-Version does not begin with \"SIP\"\n");
	return MALFORMED_SIP_MSG;
    }
    c += SIP_len;

    if(memcmp(c,SUP_SIPVER,SUP_SIPVER_len) != 0){
	DBG("Unsupported or malformed SIP-Version\n");
	return MALFORMED_SIP_MSG;
    }

    //DBG("SIP-Version OK\n");
    return 0;
}

static int _parse_gen_params(list<sip_avp*>* params, const char** c, 
			     int len, char stop_char, bool beg_w_sc)
{
    enum {
	VP_PARAM_SEP=0,
	VP_PARAM_SEP_SWS,
	VP_PNAME,
	VP_PNAME_EQU,
	VP_PNAME_EQU_SWS,
	VP_PVALUE,
	VP_PVALUE_QUOTED
    };

    const char* beg = *c;
    const char* end = beg+len;
    int saved_st=0;

    int st = beg_w_sc ? VP_PARAM_SEP : VP_PARAM_SEP_SWS;

    auto_ptr<sip_avp> avp(new sip_avp());

    for(;*c!=end;(*c)++){

	switch(st){

	case VP_PARAM_SEP:
	    switch(**c){

	    case_CR_LF;

	    case SP:
	    case HTAB:
		break;
		
	    case ';':
		st = VP_PARAM_SEP_SWS;
		break;

	    default:
		if(**c == stop_char){
		    return 0;
		}

		DBG("';' expected, found '%c'\n",**c);
		return MALFORMED_SIP_MSG;
	    }
	    break;

	case VP_PARAM_SEP_SWS:
	    switch(**c){

	    case_CR_LF;

	    case SP:
	    case HTAB:
		break;

	    default:
		st = VP_PNAME;
		beg=*c;
		break;
	    }
	    break;
	    
	case VP_PNAME:
	    switch(**c){

	    case_CR_LF;

	    case SP:
	    case HTAB:
		st = VP_PNAME_EQU;
		avp->name.set(beg,*c-beg);
		break;

	    case '=':
		st = VP_PNAME_EQU_SWS;
		avp->name.set(beg,*c-beg);
		break;

	    case ';':
		st = VP_PARAM_SEP_SWS;
		avp->name.set(beg,*c-beg);
		params->push_back(avp.release());
		avp.reset(new sip_avp());
		break;

	    default:
		if(**c == stop_char){
		    avp->name.set(beg,*c-beg);
		    params->push_back(avp.release());
		    return 0;
		}
		break;

	    }
	    break;

	case VP_PNAME_EQU:
	    switch(**c){

	    case_CR_LF;

	    case SP:
	    case HTAB:
		break;

	    case '=':
		st = VP_PNAME_EQU_SWS;
		break;

	    case ';':
		st = VP_PARAM_SEP_SWS;
		params->push_back(avp.release());
		avp.reset(new sip_avp());
		break;

	    default:
		if(**c == stop_char){
		    params->push_back(avp.release());
		    return 0;
		}
		DBG("'=' expected\n");
		return MALFORMED_SIP_MSG;
	    }

	case VP_PNAME_EQU_SWS:
	    switch(**c){

	    case_CR_LF;

	    case SP:
	    case HTAB:
		break;
		
	    case '\"':
		st = VP_PVALUE_QUOTED;
		beg = *c;
		break;

	    case ';':
		st = VP_PARAM_SEP_SWS;
		params->push_back(avp.release());
		avp.reset(new sip_avp());
		break;

	    default:
		st = VP_PVALUE;
		beg = *c;
		break;
	    }
	    break;

	case VP_PVALUE:
	    switch(**c){

	    case_CR_LF;

	    case '\"':
		st = VP_PVALUE_QUOTED;
		break;

	    case ';':
		st = VP_PARAM_SEP_SWS;
		avp->value.set(beg,*c-beg);
		params->push_back(avp.release());
		avp.reset(new sip_avp());
		break;

	    default:
		if(**c == stop_char){
		    avp->value.set(beg,*c-beg);
		    params->push_back(avp.release());
		    return 0;
		}
		break;
	    }
	    break;

	case VP_PVALUE_QUOTED:
	    switch(**c){

	    case_CR_LF;

	    case '\"':
		st = VP_PARAM_SEP;
		avp->value.set(beg,*c+1-beg);
		params->push_back(avp.release());
		avp.reset(new sip_avp());
		break;
		
	    case '\\':
		if(!*(++(*c))){
		    DBG("Escape char in quoted str at EoT!!!\n");
		    return MALFORMED_SIP_MSG;
		}
		break;
	    }
	    break;

	case_ST_CR(**c);
	
	case ST_LF:
	case ST_CRLF:
	    switch(saved_st){

	    case VP_PNAME:
		saved_st = VP_PNAME_EQU;
		avp->name.set(beg,*c-(st==ST_CRLF?2:1)-beg);
		break;

	    case VP_PVALUE:
		saved_st = VP_PARAM_SEP;
		avp->value.set(beg,*c-(st==ST_CRLF?2:1)-beg);
		params->push_back(avp.release());
		avp.reset(new sip_avp());
		break;
	    }
	    st = saved_st;
	}
    }
    
    switch(st){

    case VP_PNAME:
	avp->name.set(beg,*c-beg);
	params->push_back(avp.release());
	break;

    case VP_PVALUE:
	avp->value.set(beg,*c-beg);
	params->push_back(avp.release());
	break;

    case VP_PARAM_SEP:
    case VP_PARAM_SEP_SWS:
	break;

    case VP_PNAME_EQU:
    case VP_PNAME_EQU_SWS:
	params->push_back(avp.release());
	break;

    default:
	DBG("Wrong state: st=%i\n",st);
	return MALFORMED_SIP_MSG;
    }

    return 0;
}

int parse_gen_params_sc(list<sip_avp*>* params, const char** c, 
			int len, char stop_char)
{
    return _parse_gen_params(params,c,len,stop_char,true);
}

int parse_gen_params(list<sip_avp*>* params, const char** c,
		     int len, char stop_char)
{
    return _parse_gen_params(params,c,len,stop_char,false);
}

void free_gen_params(list<sip_avp*>* params)
{
    while(!params->empty()) {
	delete params->front();
	params->pop_front();
    }
}

/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
