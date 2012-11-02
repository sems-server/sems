/*
 * $Id: parse_from_to.cpp 850 2008-04-04 21:29:36Z sayer $
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

#include "parse_from_to.h"
#include "parse_common.h"
#include "log.h"


int parse_from_to(sip_from_to* ft, const char* beg, int len)
{
    enum {
	FTP_BEG,

	FTP_TAG1,
	FTP_TAG2,
	FTP_TAG3,

	FTP_OTHER
    };

    const char* c = beg;
    int ret = parse_nameaddr(&ft->nameaddr,&c,len);
    if(ret) return ret;
    
    if(!ft->nameaddr.params.empty()){

	list<sip_avp*>::iterator it = ft->nameaddr.params.begin();
	for(;it!=ft->nameaddr.params.end();++it){

	    const char* c = (*it)->name.s;
	    const char* end = c + (*it)->name.len;
	    int st = FTP_BEG;
	    
	    for(;c!=end;c++){

#define case_FT_PARAM(st1,ch1,ch2,st2)\
	    case st1:\
		switch(*c){\
		case ch1:\
		case ch2:\
		    st = st2;\
		    break;\
		default:\
		    st = FTP_OTHER;\
		}\
		break

		switch(st){
		    case_FT_PARAM(FTP_BEG, 't','T',FTP_TAG1);
		    case_FT_PARAM(FTP_TAG1,'a','A',FTP_TAG2);
		    case_FT_PARAM(FTP_TAG2,'g','G',FTP_TAG3);

		case FTP_OTHER:
		    goto next_param;
		}
	    }

	    switch(st){
	    case FTP_TAG3:
		ft->tag = (*it)->value;
		break;
	    }

	next_param:
	    continue;
	}
    }
    
    return ret;
}

/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
