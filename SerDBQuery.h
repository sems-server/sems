/*
 * $Id: SerDBQuery.h,v 1.1 2004/09/21 14:50:50 rco Exp $
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

#ifndef _SerDBQuery_h_
#define _SerDBQuery_h_

#include <string>
#include <vector>
using std::string;
using std::vector;

class SerDBQuery
{
    string         tab_name;

    vector<string> keys;
    vector<string> where_clauses;

    vector<string>          cols;
    vector<vector<string> > vals;

    int parseLine(char*& str, vector<string>& line, 
		  bool first=false);

    int parseResult(char* str);

public:
    SerDBQuery(const string& tab_name);

    void addKey(const string& key) 
	{ keys.push_back(key); }
    
    void addWhereClause(const string& where)
	{ where_clauses.push_back(where); }
    
    int  execute();

    int getCols() {return cols.size();}
    int getLines() {return vals.size();}
    int getVals(int line) {return vals[line].size();}
    
    string getCol(int col) {return cols[col];}
    string getVal(int line, int val) {return vals[line][val];}
    vector<string>& getLine(int line) {return vals[line];}
};

#endif
// Local Variables:
// mode:C++
// End:

