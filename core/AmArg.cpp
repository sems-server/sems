/*
 * $Id: AmArg.h 368 2007-06-12 18:24:33Z sayer $
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

AmArg::AmArg(const AmArg& v)
{  
  type = Undef;

  if (this != &v) {
    invalidate();
    
    type = v.type;
    switch(type){
    case Int: { v_int = v.v_int; } break;
    case Double: { v_double = v.v_double; } break;
    case CStr: { v_cstr = strdup(v.v_cstr); } break;
    case AObject:{  v_obj = v.v_obj; } break;
    case Array:  { v_array = new ValueArray(*v.v_array); } break;
    case Undef: break;
    default: assert(0);
    }
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

void AmArg::invalidate() {
  if(type == CStr) { free((void*)v_cstr); }
  if(type == Array) { delete v_array; }
    
  type = Undef;
}

void AmArg::push(const AmArg& a) {
  assertArray();
  v_array->push_back(a);
}

const size_t AmArg::size() const {
  assertArray();  
  return v_array->size(); 
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

