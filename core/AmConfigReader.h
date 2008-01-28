/*
 * $Id$
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
/** @file AmConfigReader.h */
#ifndef AmConfigReader_h
#define AmConfigReader_h

#include <string>
#include <map>
using std::string;


#define MAX_CONFIG_LINE 512
/**
 * \brief configuration file reader
 * 
 * Reads configuration file into internal map, 
 * which subsequently can be queried for the value of
 * specific configuration values.
 */

class AmConfigReader
{
  std::map<string,string> keys;

 public:
  int  loadFile(const string& path);
  bool hasParameter(const string& param);
  const string& getParameter(const string& param, const string& defval = "");
  unsigned int getParameterInt(const string& param, unsigned int defval = 0);

  std::map<string,string>::const_iterator begin() const
    { return keys.begin(); }

  std::map<string,string>::const_iterator end() const
    { return keys.end(); }
};

#endif
