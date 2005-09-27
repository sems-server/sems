/*
 * $Id: SerDBQuery.cpp,v 1.1 2004/09/21 14:50:50 rco Exp $
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

#include "SerDBQuery.h"
#include "SerClient.h"
#include "AmServer.h"
#include "AmUtils.h"
#include "AmSession.h"
#include "log.h"

inline string vec2str(const vector<string>& v, char sep)
{
    if(!v.size())
	return "";

    vector<string>::const_iterator it = v.begin();
    string res = *(it++);

    for(; it != v.end(); ++it)
	res += sep + *it;

    return res;
}

SerDBQuery::SerDBQuery(const string& tab_name)
    : tab_name(tab_name)
{
}

int  SerDBQuery::execute()
{
    string keys_str = vec2str(keys,'\n');
    string wc_str = vec2str(where_clauses,'\n');
    string query =
	// ":DB:" + id + "\n"
	"select\n" + 
	string(keys_str.empty() ? "" : keys_str + "\n") +
	".\n" +
	tab_name + "\n" +
	string(wc_str.empty() ? "" : wc_str + "\n") +
	".\n\n";

    SerClient* client = SerClient::getInstance();

    int id = client->open();
    if(id == -1){
	ERROR("could not open Ser client\n");
	return -1;
    }

    char* res_buf=0;
    int   ret=-1;

    if(client->send(id,"DB",query,SER_DBREQ_TIMEOUT) == -1){
	ERROR("while sending query to Ser\n");
	goto error;
    }

    res_buf = client->getResponseBuffer(id);
    if(!res_buf){
	ERROR("while extracting Ser's response buffer\n");
	goto error;
    }

    ret = parseResult(res_buf);

 error:
    client->close(id);
    return ret;
}

int SerDBQuery::parseResult(char* str)
{
    int ret = parseLine(str,cols,true);
    while(ret > 0){
	vector<string> line;
	ret = parseLine(str,line);
	if(ret>0)
	    vals.push_back(line);
    }
    
    return (ret == -1 ? ret : vals.size());
}

int SerDBQuery::parseLine(char*& str, vector<string>& line, bool first)
{
    char buf[MAX_LINE_SIZE] = "";

    int ret = msg_get_line(str, buf, MAX_LINE_SIZE);
    if(ret==-1){
	ERROR("Buffer overflow\n");
	return -1;
    }

    DBG("line=<%s>\n",buf);
    
    char* s=buf;
    char* c=s;
    
    for(; *c; s=++c){
	while(*c != '\0' && *c != ' ') c++;
	if(c!=s){
	    *c = '\0';
	    line.push_back(s);
	}
    }
    
    return line.size();
}
