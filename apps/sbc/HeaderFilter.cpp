/*
 * Copyright (C) 2010 Stefan Sayer
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include "HeaderFilter.h"
#include "sip/parse_common.h"
#include "log.h"
#include <algorithm>

const char* FilterType2String(FilterType ft) {
    switch(ft) {
    case Transparent: return "transparent";
    case Whitelist: return "whitelist";
    case Blacklist: return "blacklist";
    default: return "unknown";
    };
}

FilterType String2FilterType(const char* ft) {
    if (!ft)
	return Undefined;

    if (!strcasecmp(ft,"transparent"))
	return Transparent;

    if (!strcasecmp(ft,"whitelist"))
	return Whitelist;

    if (!strcasecmp(ft,"blacklist"))
	return Blacklist;

    return Undefined;
}

bool isActiveFilter(FilterType ft) {
    return (ft != Undefined) && (ft != Transparent);
}

int skip_header(const std::string& hdr, size_t start_pos, 
		 size_t& name_end, size_t& val_begin,
		 size_t& val_end, size_t& hdr_end) {
    // adapted from sip/parse_header.cpp

    name_end = val_begin = val_end = start_pos;
    hdr_end = hdr.length();

    //
    // Header states
    //
    enum {
	H_NAME=0,
	H_HCOLON,
	H_VALUE_SWS,
	H_VALUE,
    };

    int st = H_NAME;
    int saved_st = 0;

    
    size_t p = start_pos;
    for(;p<hdr.length() && st != ST_LF && st != ST_CRLF;p++){

	switch(st){

	case H_NAME:
	    switch(hdr[p]){

	    case_CR_LF;

	    case HCOLON:
		st = H_VALUE_SWS;
		name_end = p;
		break;

	    case SP:
	    case HTAB:
		st = H_HCOLON;
		name_end = p;
		break;
	    }
	    break;

	case H_VALUE_SWS:
	    switch(hdr[p]){

		case_CR_LF;

	    case SP:
	    case HTAB:
		break;

	    default:
		st = H_VALUE;
		val_begin = p;
		break;
		
	    };
	    break;

	case H_VALUE:
	    switch(hdr[p]){
		case_CR_LF;
	    };
	    if (st==ST_CR || st==ST_LF)
		val_end = p;
	    break;

	case H_HCOLON:
	    switch(hdr[p]){
	    case HCOLON:
		st = H_VALUE_SWS;
		val_begin = p;
		break;

	    case SP:
	    case HTAB:
		break;

	    default:
		DBG("Missing ':' after header name\n");
		return MALFORMED_SIP_MSG;
	    }
	    break;

	case_ST_CR(hdr[p]);

	    st = saved_st;
	    hdr_end = p;
	    break;
	}
    }
    
    hdr_end = p;
    if (p==hdr.length() && st==H_VALUE) {
	val_end = p;	
    }
    
    return 0;
}

int inplaceHeaderFilter(string& hdrs, const set<string>& headerfilter_list, FilterType f_type) {
   if (!hdrs.length() || !isActiveFilter(f_type))
	return 0;

    int res = 0;
    size_t start_pos = 0;
    while (start_pos<hdrs.length()) {
	size_t name_end, val_begin, val_end, hdr_end;
	if ((res = skip_header(hdrs, start_pos, name_end, val_begin,
			       val_end, hdr_end)) != 0) {
	    return res;
	}
	string hdr_name = hdrs.substr(start_pos, name_end-start_pos);
	transform(hdr_name.begin(), hdr_name.end(), hdr_name.begin(), ::tolower);
	bool erase = false;
	if (f_type == Whitelist) {
	    erase = headerfilter_list.find(hdr_name)==headerfilter_list.end();
	} else if (f_type == Blacklist) {
	    erase = headerfilter_list.find(hdr_name)!=headerfilter_list.end();
	}
	if (erase) {
	    DBG("erasing header '%s'\n", hdr_name.c_str());
	    hdrs.erase(start_pos, hdr_end-start_pos);
	} else {
	    start_pos = hdr_end;
	}
    }

    // todo: multi-line header support

    return res;
}

/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
