/*
 * $Id: ModMysql.cpp 1764 2010-04-01 14:33:30Z peter_lemenkov $
 *
 * Copyright (C) 2010 TelTech Systems Inc.
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

#include "AmArg.h"
#include "AmUtils.h"
#include "log.h"

#include "jsonArg.h"
using std::string;

#include "jsonxx.h"
using namespace jsonxx;

#include <sstream>

const char *hex_chars = "0123456789abcdef";

string str2json(const char* str)
{
  return str2json(str,strlen(str));
}

string str2json(const string& str)
{
  return str2json(str.c_str(),str.length());
}

string str2json(const char* str, size_t len)
{
    // borrowed from jsoncpp
    // Not sure how to handle unicode...
    if (strpbrk(str, "\"\\\b\f\n\r\t") == NULL)
      return string("\"") + str + "\"";
    // We have to walk value and escape any special characters.
    // Appending to std::string is not efficient, but this should be rare.
    // (Note: forward slashes are *not* rare, but I am not escaping them.)
    unsigned maxsize = len*2 + 3; // allescaped+quotes+NULL
    std::string result;
    result.reserve(maxsize); // to avoid lots of mallocs
    result += "\"";
    const char* end = str + len;
    for (const char* c = str; (c != end) && (*c != 0); ++c){
      switch(*c){
      case '\"':
	result += "\\\"";
	break;
      case '\\':
	result += "\\\\";
	break;
      case '\b':
	result += "\\b";
	break;
      case '\f':
	 result += "\\f";
	 break;
      case '\n':
	result += "\\n";
	break;
      case '\r':
	result += "\\r";
	break;
      case '\t':
	result += "\\t";
	break;
      case '/':
	// Even though \/ is considered a legal escape in JSON, a bare
	// slash is also legal, so I see no reason to escape it.
	// (I hope I am not misunderstanding something.)
      default:{
	if (*c < ' ') 
	  result += "\\u00" + hex_chars[*c >> 4] + hex_chars[*c & 0xf];
	else 
	  result += *c;
      } break;
      }
    }
    result += "\"";
    return result;
}

string arg2json(const AmArg &a) {
  // TODO: optimize to avoid lots of mallocs
  // TODO: how to get a bool? 
  string s;
  switch (a.getType()) {
  case AmArg::Undef:
    return "null";

  case AmArg::Int:
    return a.asInt()<0?"-"+int2str(abs(a.asInt())):int2str(abs(a.asInt()));

  case AmArg::LongLong:
    return longlong2str(a.asLongLong());

  case AmArg::Bool:
    return a.asBool()?"true":"false";

  case AmArg::Double: 
    return double2str(a.asDouble());

  case AmArg::CStr:
    return str2json(a.asCStr());

  case AmArg::Array:
    s = "[";
    for (size_t i = 0; i < a.size(); i ++)
      s += arg2json(a[i]) + ", ";
    if (1 < s.size()) 
      s.resize(s.size() - 2); // strip last ", "
    s += "]";
    return s;

  case AmArg::Struct:
    s = "{";
    for (AmArg::ValueStruct::const_iterator it = a.asStruct()->begin();
	 it != a.asStruct()->end(); it ++) {
      s += '"'+it->first + "\": ";
      s += arg2json(it->second);
      s += ", ";
    }
    if (1 < s.size())
      s.resize(s.size() - 2); // strip last ", "
    s += "}";
    return s;
  default: break;
  }

  return "{}";
}

// based on jsonxx
bool array_parse(std::istream& input, AmArg& res) {
  if (!match("[", input)) {
    return false;
  }

  res.assertArray();

  if (match("]", input)) {
    return true;
  }

  do {
    res.push(AmArg());
    AmArg v;
    if (!json2arg(input, res.get(res.size()-1))) {
      res.clear();
      return false;
      res.pop_back();
      break; // TODO: return false????
    }
  } while (match(",", input));
  
  if (!match("]", input)) {
    res.clear();
    return false;
  }
  return true;
}

bool object_parse(std::istream& input, AmArg& res) {
  if (!match("{", input)) {
    return false;
  }

  res.assertStruct();

  if (match("}", input)) {
    return true;
  }

  do {
    std::string key;
    if (!parse_string(input, &key)) {
      if (match("}",input,true)) {          
	return true;
      }
      res.clear();
      return false;
    }
    if (!match(":", input)) {
      res.clear();
      return false;
    }
    res[key] = AmArg(); // using the reference
    if (!json2arg(input, res[key])) {
      res.clear();
      return false;
    }
  } while (match(",", input));
  
  if (!match("}", input)) {
    res.clear();
    return false;
  }

  return true;
}

bool json2arg(const std::string& input, AmArg& res) {
  std::istringstream iss(input);
  return json2arg(iss, res);
}

bool json2arg(const char* input, AmArg& res) {
  std::istringstream iss(input);
  return json2arg(iss, res);
}

bool json2arg(std::istream& input, AmArg& res) {

  res.clear();

  std::string string_value;
  if (parse_string(input, &string_value)) {
    res = string_value; // todo: unnecessary value copy here
    return true;
  }

  if (parse_float(input, &res.v_double)) {
    res.type = AmArg::Double;
    return true;
  }

  if (parse_number(input, &res.v_int)) {
    res.type = AmArg::Int;
    return true;
  }
  
  if (parse_bool(input, &res.v_bool)) {
    res.type = AmArg::Bool;
    return true;
  }

  if (parse_null(input)) { // AmArg::Undef
    return true;
  }

  if (array_parse(input, res)) {
    return true;
  }

  if (object_parse(input, res)) {
     return true;
  }

  res.clear();
  return false;
}
