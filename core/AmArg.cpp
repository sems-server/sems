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

#include "AmArg.h"
#include "log.h"
#include "AmUtils.h"

const char* t2str(int type) {
  switch (type) {
  case 0: return "Undef";
  case 1: return "Int";
  case 2: return "Double";
  case 3: return "CStr";
  case 4: return "AObject";
  case 5: return "Blob";
  case 6: return "Array";
  case 7: return "Struct";
  default: return "unknown";
  }
}

AmArg::AmArg(const AmArg& v)
{ 
  type = Undef;

  *this = v;
}

AmArg& AmArg::operator=(const AmArg& v) {
  if (this != &v) {
    invalidate();
    
    type = v.type;
    switch(type){
    case Int:    { v_int = v.v_int; } break;
    case Double: { v_double = v.v_double; } break;
    case CStr:   { v_cstr = strdup(v.v_cstr); } break;
    case AObject:{ v_obj = v.v_obj; } break;
    case Array:  { v_array = new ValueArray(*v.v_array); } break;
    case Struct: { v_struct = new ValueStruct(*v.v_struct); } break;
    case Blob:   {  v_blob = new ArgBlob(*v.v_blob); } break;
    case Undef: break;
    default: assert(0);
    }
  }
  return *this;
}

AmArg::AmArg(std::map<std::string, std::string>& v) 
  : type(Undef) {
  assertStruct();
  for (std::map<std::string, std::string>::iterator it=
	 v.begin();it!= v.end();it++)
    (*v_struct)[it->first] = AmArg(it->second.c_str());
}

AmArg::AmArg(std::map<std::string, AmArg>& v) 
  : type(Undef) {
  assertStruct();
  for (std::map<std::string, AmArg>::iterator it=
	 v.begin();it!= v.end();it++)
    (*v_struct)[it->first] = it->second;
}

AmArg::AmArg(vector<std::string>& v)
  : type(Array) {
  assertArray(0);
  for (vector<std::string>::iterator it 
	 = v.begin(); it != v.end(); it++) {
    push(AmArg(it->c_str()));
  }
}
    
AmArg::AmArg(const vector<int>& v ) 
  : type(Array) {
  assertArray(0);
  for (vector<int>::const_iterator it 
	 = v.begin(); it != v.end(); it++) {
    push(AmArg(*it));
  }
}

AmArg::AmArg(const vector<double>& v)
  : type(Array) {
  assertArray(0);
  for (vector<double>::const_iterator it 
	 = v.begin(); it != v.end(); it++) {
    push(AmArg(*it));
  }
}

void AmArg::assertArray() {
  if (Array == type)
    return;
  if (Undef == type) {
    type = Array;
    v_array = new ValueArray();
    return;
  } 
  throw TypeMismatchException();
}

void AmArg::assertArray() const {
  if (Array != type)
    throw TypeMismatchException();
}

void AmArg::assertArray(size_t s) {
    
  if (Undef == type) {
    type = Array;
    v_array = new ValueArray();
  } else if (Array != type) {
    throw TypeMismatchException();
  }
  if (v_array->size() < s)
    v_array->resize(s);
}

void AmArg::assertStruct() {
  if (Struct == type)
    return;
  if (Undef == type) {
    type = Struct;
    v_struct = new ValueStruct();
    return;
  } 
  throw TypeMismatchException();
}

void AmArg::assertStruct() const {
  if (Struct != type)
    throw TypeMismatchException();
}

void AmArg::invalidate() {
  if(type == CStr) { free((void*)v_cstr); }
  else if(type == Array) { delete v_array; }
  else if(type == Struct) { delete v_struct; }
  else if(type == Blob) { delete v_blob; }
  type = Undef;
}

void AmArg::push(const AmArg& a) {
  assertArray();
  v_array->push_back(a);
}

void AmArg::push(const string &key, const AmArg &val) {
  assertStruct();
  (*v_struct)[key] = val;
}

void AmArg::pop(AmArg &a) {
  assertArray();
  if (!size()) {
    if (a.getType() == AmArg::Undef) 
      return;
    a = AmArg();
    return;
  }
  a = v_array->front();
  v_array->erase(v_array->begin());
}

void AmArg::concat(const AmArg& a) {
  assertArray();
  if (a.getType() == Array) { 
  for (size_t i=0;i<a.size();i++)
    v_array->push_back(a[i]);
  } else {
    v_array->push_back(a);
  }
}

const size_t AmArg::size() const {
  if (Array == type)
    return v_array->size(); 

  if (Struct == type)
    return v_struct->size(); 

  throw TypeMismatchException();
}

AmArg& AmArg::get(size_t idx) {
  assertArray();  
  if (idx >= v_array->size())
    throw OutOfBoundsException();
    
  return (*v_array)[idx];
}

AmArg& AmArg::get(size_t idx) const {
  assertArray();  
  if (idx >= v_array->size())
    throw OutOfBoundsException();
    
  return (*v_array)[idx];
}

AmArg& AmArg::operator[](size_t idx) { 
  assertArray(idx+1); 
  return (*v_array)[idx];
}

AmArg& AmArg::operator[](size_t idx) const { 
  assertArray();  
  if (idx >= v_array->size())
    throw OutOfBoundsException();
    
  return (*v_array)[idx];
}

AmArg& AmArg::operator[](int idx) { 
  if (idx<0)
    throw OutOfBoundsException();

  assertArray(idx+1); 
  return (*v_array)[idx];
}

AmArg& AmArg::operator[](int idx) const { 
  if (idx<0)
    throw OutOfBoundsException();

  assertArray();  
  if ((size_t)idx >= v_array->size())
    throw OutOfBoundsException();
    
  return (*v_array)[idx];
}

AmArg& AmArg::operator[](std::string key) {
  assertStruct();
  return (*v_struct)[key];
}

AmArg& AmArg::operator[](std::string key) const {
  assertStruct();
  return (*v_struct)[key];
}

AmArg& AmArg::operator[](const char* key) {
  assertStruct();
  return (*v_struct)[key];
}

AmArg& AmArg::operator[](const char* key) const {
  assertStruct();
  return (*v_struct)[key];
}

bool operator==(const AmArg& lhs, const AmArg& rhs) {
  if (lhs.type != rhs.type)
    return false;

  switch(lhs.type){
  case AmArg::Int:    { return lhs.v_int == rhs.v_int; } break;
  case AmArg::Double: { return lhs.v_double == rhs.v_double; } break;
  case AmArg::CStr:   { return !strcmp(lhs.v_cstr,rhs.v_cstr); } break;
  case AmArg::AObject:{ return lhs.v_obj == rhs.v_obj; } break;
  case AmArg::Array:  { return lhs.v_array == rhs.v_array;  } break;
  case AmArg::Struct: { return lhs.v_struct == rhs.v_struct;  } break;
  case AmArg::Blob:   {  return (lhs.v_blob->len == rhs.v_blob->len) &&  
	!memcmp(lhs.v_blob->data, rhs.v_blob->data, lhs.v_blob->len); } break;
  case AmArg::Undef:  return true;
  default: assert(0);
  }
}

bool AmArg::hasMember(const char* name) const {
  return type == Struct && v_struct->find(name) != v_struct->end();
}

bool AmArg::hasMember(const string& name) const {
  return type == Struct && v_struct->find(name) != v_struct->end();
}

std::vector<std::string> AmArg::enumerateKeys() const {
  assertStruct();
  std::vector<std::string> res;
  for (ValueStruct::iterator it = 
	 v_struct->begin(); it != v_struct->end(); it++)
    res.push_back(it->first);
  return res;
}

AmArg::ValueStruct::const_iterator AmArg::begin() const {
  assertStruct();
  return v_struct->begin();
}

AmArg::ValueStruct::const_iterator AmArg::end() const {
  assertStruct();
  return v_struct->end();
}

void AmArg::erase(const char* name) {
  assertStruct();
  v_struct->erase(name);
}

void AmArg::assertArrayFmt(const char* format) const {
  size_t fmt_len = strlen(format);
  string got;
  try {
    for (size_t i=0;i<fmt_len;i++) {
      switch (format[i]) {
      case 'i': assertArgInt(get(i)); got+='i';  break;
      case 'f': assertArgDouble(get(i)); got+='f'; break;
      case 's': assertArgCStr(get(i)); got+='s'; break;
      case 'o': assertArgAObject(get(i)); got+='o'; break;
      case 'a': assertArgArray(get(i)); got+='a'; break;
      case 'b': assertArgBlob(get(i)); got+='b'; break;
      case 'u': assertArgStruct(get(i)); got+='u'; break;
      default: got+='?'; ERROR("ignoring unknown format type '%c'\n", 
			       format[i]); break;
      }
    }
  } catch (...) {
    ERROR("parameter mismatch: expected '%s', got '%s...'\n",
	  format, got.c_str());
    throw;
  }
}

#define VECTOR_GETTER(type, name, getter)	\
  vector<type> AmArg::name() const {		\
    vector<type> res;				\
    for (size_t i=0;i<size();i++)		\
      res.push_back(get(i).getter());		\
    return res;					\
  }			
VECTOR_GETTER(string, asStringVector, asCStr)
VECTOR_GETTER(int, asIntVector, asInt)
VECTOR_GETTER(double, asDoubleVector, asDouble)
VECTOR_GETTER(ArgObject*, asArgObjectVector, asObject)
#undef  VECTOR_GETTER

vector<ArgBlob> AmArg::asArgBlobVector() const {		
  vector<ArgBlob> res;				
  for (size_t i=0;i<size();i++)		
    res.push_back(*get(i).asBlob());		
  return res;					
}			

void AmArg::clear() {
  invalidate();
}

string AmArg::print(const AmArg &a) {
  string s;
  switch (a.getType()) {
    case Undef:
      return "<UNDEFINED>";
    case Int:
      return int2str(a.asInt());
    case Double:
      return double2str(a.asDouble());
    case CStr:
      return "'" + string(a.asCStr()) + "'";
    case Array:
      s = "[";
      for (size_t i = 0; i < a.size(); i ++)
        s += print(a[i]) + ", ";
      if (1 < s.size())
        s.resize(s.size() - 2); // strip last ", "
      s += "]";
      return s;
    case Struct:
      s = "{";
      for (AmArg::ValueStruct::const_iterator it = a.asStruct()->begin();
          it != a.asStruct()->end(); it ++) {
        s += "'"+it->first + "': ";
        s += print(it->second);
        s += ", ";
      }
      if (1 < s.size())
        s.resize(s.size() - 2); // strip last ", "
      s += "}";
      return s;
    default: break;
  }
  return "<UNKONWN TYPE>";
}
