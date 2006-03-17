/*
 * $Id: AmCmd.cpp,v 1.7.8.2 2005/08/25 06:55:12 rco Exp $
 *
 * Copyright (C) 2002-2003 Fhg Fokus
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

#include "AmCmd.h"
#include "log.h"

void AmCmd::addHeader(const string& hdr) {
  hdrs+=hdr;
}

string AmCmd::getHeader(const string& hdr_name) const
{
  size_t pos1; 
  size_t pos2;
  size_t pos_s;
  if (findHeader(hdr_name, pos1, pos2, pos_s)) 
    return hdrs.substr(pos1,pos2-pos1);
  else
    return "";
}

bool AmCmd::stripHeader(const string& hdr_name) 
{
  size_t pos1; 
  size_t pos2;
  size_t pos_s;
  if (findHeader(hdr_name, pos1, pos2, pos_s)) {
    hdrs.erase(pos_s,pos2);
    return true;
  } 
  
  return false;
}

bool AmCmd::findHeader(const string& hdr_name, size_t& pos1, size_t& pos2, size_t& hdr_start) const 
{
    unsigned int p;
    char* hdr = strdup(hdr_name.c_str());
    const char* hdrs_c = hdrs.c_str();
    char* hdr_c = hdr;
    const char* hdrs_end = hdrs_c + hdrs.length();
    char* hdr_end = hdr_c + hdr_name.length(); 

    while(hdr_c != hdr_end){
	if('A' <= *hdr_c && *hdr_c <= 'Z')
	    *hdr_c -= 'A' - 'a';
	hdr_c++;
    }

    while(hdrs_c != hdrs_end){

	hdr_c = hdr;

	while((hdrs_c != hdrs_end) && (hdr_c != hdr_end)){

	    char c = *hdrs_c;
	    if('A' <= *hdrs_c && *hdrs_c <= 'Z')
		c -= 'A' - 'a';

	    if(c != *hdr_c)
		break;

	    hdr_c++;
	    hdrs_c++;
	}

	if(hdr_c == hdr_end)
	    break;

	while((hdrs_c != hdrs_end) && (*hdrs_c != '\n'))
	    hdrs_c++;

	if(hdrs_c != hdrs_end)
	    hdrs_c++;
    }
    
    if(hdr_c == hdr_end){
        hdr_start = hdrs_c - hdrs.c_str();;

	while((hdrs_c != hdrs_end) && (*hdrs_c == ' '))
	    hdrs_c++;

	if((hdrs_c != hdrs_end) && (*hdrs_c == ':')){

	    hdrs_c++;
	    while((hdrs_c != hdrs_end) && (*hdrs_c == ' '))
		hdrs_c++;
	    
	    p = hdrs_c - hdrs.c_str();
	    
	    string::size_type p_end;
	    if((p_end = hdrs.find('\n',p)) != string::npos){
		
		free(hdr);
		// return hdrs.substr(p,p_end-p);
		pos1 = p;
		pos2 = p_end;
		return true;
	    }
	}
    }

    free(hdr);
    //    return "";
    return false;
}
