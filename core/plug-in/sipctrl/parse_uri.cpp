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
#include "parse_uri.h"
#include "log.h"

sip_uri::~sip_uri()
{
    list<sip_avp*>::iterator it;
    
    for(it = params.begin();
	it != params.end(); ++it) {

	delete *it;
    }

    for(it = hdrs.begin();
	it != hdrs.end(); ++it) {

	delete *it;
    }
}


static int parse_sip_uri(sip_uri* uri, char* beg, int len)
{
    enum {
	URI_USER=0, 
	URI_PW,
	URI_HOST,
	URI_PORT,
	URI_PNAME,
	URI_PVALUE,
	URI_HNAME,
	URI_HVALUE
    };

    int st  = URI_HOST;
    char* c = beg;
    //int escaped = 0;

    cstring tmp1, tmp2;

    // Search for '@', so that we can decide
    // wether to start in URI_USER or URI_HOST state.
    // This is not very efficient, but it makes the 
    // parser much easier!

    for(;c!=beg+len;c++){
	// user part present in URI
	if(*c == '@') {
	    st = URI_USER;
	}
    }

    if(st == URI_USER) {
	uri->user.s = beg;
    }
    else {
	uri->host.s = beg;
    }

    c = beg;


    for(;c!=beg+len;c++){

	switch(*c){

	case HCOLON:
	    switch(st){

	    case URI_USER:
		uri->user.len = c - uri->user.s;
		if(!uri->user.len){
		    DBG("Password given for empty user!\n");
		    return MALFORMED_URI;
		}
		uri->passwd.s = c+1;
		st = URI_PW;
		break;

	    case URI_HOST:
		uri->host.len = c - uri->host.s;
		if(!uri->host.len){
		    DBG("Empty host part\n");
		    return MALFORMED_URI;
		}
		uri->port_str.s = c+1;
		st = URI_PORT;
		break;
	    }
	    break;

	case '@':
	    switch(st){

	    case URI_USER:
		uri->user.len = c - uri->user.s;
		st = URI_HOST;
		uri->host.set(c+1,0);
		break;

	    case URI_PW:
		uri->passwd.len = c - uri->passwd.s; 
		st = URI_HOST;
		uri->host.set(c+1,0);
		break;

	    default:
		DBG("Illegal char '@' in non-user part\n");
		return MALFORMED_URI;
		break;
	    }
	    break;

	case ';':
	    switch(st){
	    case URI_HOST:
		uri->host.len = c - uri->host.s;
		st = URI_PNAME;
		tmp1.set(c+1,0);
		break;

	    case URI_PORT:
		uri->port_str.len = c - uri->port_str.s;
		st = URI_PNAME;
		tmp1.set(c+1,0);
		break; 

	    case URI_PNAME:
		DBG("Empty URI parameter\n");
		return MALFORMED_URI;

	    case URI_PVALUE:
		tmp2.len = c - tmp2.s;
		uri->params.push_back(new sip_avp(tmp1,tmp2));

		//DBG("uri param: \"%.*s\"=\"%.*s\"\n",
		//    tmp1.len, tmp1.s,
		//    tmp2.len, tmp2.s);

		tmp1.s = c+1;
		st = URI_PNAME;
		break;
	    }
	    break;

	case '?':
	    switch(st){

	    case URI_HOST:
		uri->host.len = c - uri->host.s;
		st = URI_HNAME;
		tmp1.s = c+1;
		break;

	    case URI_PORT:
		uri->port_str.len = c - uri->port_str.s;
		st = URI_HNAME;
		tmp1.s = c+1;
		break;

	    case URI_PNAME:
		DBG("Empty URI parameter\n");
		return MALFORMED_URI;

	    case URI_PVALUE:
		tmp2.len = c - tmp2.s;
		uri->params.push_back(new sip_avp(tmp1,tmp2));

		//DBG("uri param: \"%.*s\"=\"%.*s\"\n",
		//    tmp1.len, tmp1.s,
		//    tmp2.len, tmp2.s);

		tmp1.s = c+1;
		st = URI_HNAME;
		break;
	    }
	    break;

	case '=':
	    switch(st){
	    case URI_PNAME:
	    case URI_HNAME:
		tmp1.len = c - tmp1.s;
		if(!tmp1.len){
		    DBG("Empty param/header name\n");
		    return MALFORMED_URI;
		}
		tmp2.s = c+1;
		st++;
		break;
	    }
	    break;

	case '&':
	    switch(st){
	    case URI_HNAME:
		DBG("Empty URI header\n");
		return MALFORMED_URI;

	    case URI_HVALUE:
		tmp2.len = c - tmp2.s;
		uri->hdrs.push_back(new sip_avp(tmp1,tmp2));

		//DBG("uri hdr: \"%.*s\"=\"%.*s\"\n",
		//    tmp1.len, tmp1.s,
		//    tmp2.len, tmp2.s);

		tmp1.s = c+1;
		st = URI_HNAME;
		break;
	    }
	    break;
	}
    }

    switch(st){

    case URI_USER:
    case URI_PW:
	DBG("Missing host part\n");
	return MALFORMED_URI;

    case URI_HOST:
	uri->host.len = c - uri->host.s;
	if(!uri->host.len){
	    DBG("Missing host part\n");
	    return MALFORMED_URI;
	}
	break;

    case URI_PORT:
	uri->port_str.len = c - uri->port_str.s; 
	break;

    case URI_PNAME:
	DBG("Empty URI parameter\n");
	return MALFORMED_URI;

    case URI_PVALUE:
	tmp2.len = c - tmp2.s;
	uri->params.push_back(new sip_avp(tmp1,tmp2));
	
	//DBG("uri param: \"%.*s\"=\"%.*s\"\n",
	//    tmp1.len, tmp1.s,
	//    tmp2.len, tmp2.s);
	break;
	
    case URI_HNAME:
	DBG("Empty URI header\n");
	return MALFORMED_URI;

    case URI_HVALUE:
	tmp2.len = c - tmp2.s;
	uri->hdrs.push_back(new sip_avp(tmp1,tmp2));
	
	//DBG("uri hdr: \"%.*s\"=\"%.*s\"\n",
	//    tmp1.len, tmp1.s,
	//    tmp2.len, tmp2.s);
	break;
    }

    if(uri->port_str.len){
	uri->port = 0;
	for(int i=0; i<uri->port_str.len; i++){
	    uri->port = uri->port*10 + (uri->port_str.s[i] - '0');
	}
    }
    else {
	uri->port = 5060;
    }

    DBG("Converted URI port (%.*s) to int (%i)\n",
	uri->port_str.len,uri->port_str.s,uri->port);

    return 0;
}

int parse_uri(sip_uri* uri, char* beg, int len)
{
    enum {
	URI_BEG=0,
	SIP_S,   // Sip
	SIP_I,   // sIp
	SIP_P,   // siP
	SIPS_S   // sipS
    };

    int st = URI_BEG;
    char* c = beg;

    for(;c!=beg+len;c++){
	switch(st){
	case URI_BEG:
	    switch(*c){
	    case 's':
	    case 'S':
		st = SIP_S;
		continue;
	    default:
		DBG("Unknown URI scheme\n");
		return 0;
	    }
	    break;
	case SIP_S:
	    switch(*c){
	    case 'i':
	    case 'I':
		st = SIP_I;
		continue;
	    default:
		DBG("Unknown URI scheme\n");
		return 0;
	    }
	    break;
	case SIP_I:
	    switch(*c){
	    case 'p':
	    case 'P':
		st = SIP_P;
		continue;
	    default:
		DBG("Unknown URI scheme\n");
		return 0;
	    }
	    break;
	case SIP_P:
	    switch(*c){
	    case HCOLON:
		//DBG("scheme: sip\n");
		uri->scheme = sip_uri::SIP;
		return parse_sip_uri(uri,c+1,len-(c+1-beg));
	    case 's':
	    case 'S':
		st = SIPS_S;
		continue;
	    default:
		DBG("Unknown URI scheme\n");
		return 0;
	    }
	    break;
	case SIPS_S:
	    switch(*c){
	    case HCOLON:
		//DBG("scheme: sips\n");
		uri->scheme = sip_uri::SIPS;
		return parse_sip_uri(uri,c+1,len-(c+1-beg));
	    default:
		DBG("Unknown URI scheme\n");
		return 0;
	    }
	    break;
	default:
	    DBG("bug: unknown state\n");
	    return 1;
	}
    }

    return 0;
}
