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


#include "parse_common.h"
#include "log.h"

#include <string.h>

#include <memory>
using std::auto_ptr;

//
// SIP version constants
//

char* SIP = "SIP";
#define SIP_len 3

char* SUP_SIPVER = "2.0";
#define SUP_SIPVER_len 3


int parse_sip_version(char* beg, int len)
{
    char* c = beg;
    //char* end = c+len;

    if(len!=7){
	DBG("SIP-Version string length != SIPVER_len\n");
	return MALFORMED_SIP_MSG;
    }

    if(memcmp(c,SIP,SIP_len) != 0){
	DBG("SIP-Version does not begin with \"SIP\"\n");
	return MALFORMED_SIP_MSG;
    }
    c += SIP_len;

    if(*c++ != '/'){
	DBG("SIP-Version has no \"/\" after \"SIP\"\n");
	return MALFORMED_SIP_MSG;
    }

    if(memcmp(c,SUP_SIPVER,SUP_SIPVER_len) != 0){
	DBG("Unsupported or malformed SIP-Version\n");
	return MALFORMED_SIP_MSG;
    }

    //DBG("SIP-Version OK\n");
    return 0;
}


int parse_gen_params(list<sip_avp*>* params, char** c, int len, char stop_char)
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

    char* beg = *c;
    char* end = beg+len;
    int saved_st=0,st=VP_PARAM_SEP;

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
	break;

    default:
	DBG("Wrong state: st=%i\n",st);
	return MALFORMED_SIP_MSG;
    }

    return 0;
}
