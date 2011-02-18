/*
 * Copyright (C) 2011 Stefan Sayer
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
#include "RegexMapper.h"
#include "log.h"

bool RegexMapper::mapRegex(const string& mapping_name, const char* test_s,
			   string& result) {
  lock();
  std::map<string, RegexMappingVector>::iterator it=regex_mappings.find(mapping_name);
  if (it == regex_mappings.end()) {
    unlock();
    ERROR("regex mapping '%s' is not loaded!\n", mapping_name.c_str());
    return false;
  }

  bool res = run_regex_mapping(it->second, test_s, result);
  unlock();
  return res;
}

void RegexMapper::setRegexMap(const string& mapping_name, const RegexMappingVector& r) {
  lock();
  std::map<string, RegexMappingVector>::iterator it=regex_mappings.find(mapping_name);
  if (it != regex_mappings.end()) {
    for (RegexMappingVector::iterator r_it =
	   it->second.begin(); r_it != it->second.end(); r_it++) {
      regfree(&r_it->first);
    }
  }
  regex_mappings[mapping_name] = r;
  unlock();
}

std::vector<std::string> RegexMapper::getNames() {
  std::vector<std::string> res;
  lock();
  for (std::map<string, RegexMappingVector>::iterator it=
	 regex_mappings.begin(); it != regex_mappings.end(); it++)
    res.push_back(it->first);
  unlock();
  return res;
}

