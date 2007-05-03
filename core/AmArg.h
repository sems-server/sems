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

#ifndef _AmArg_h_
#define _AmArg_h_

#include <assert.h>
#include <string.h>

#include <vector>
using std::vector;

/** base for Objects as @see AmArg parameter*/
class ArgObject {
 public:
  ArgObject() { }
  virtual ~ArgObject() { }
};

/** \brief variable type argument for DynInvoke APIs */
class AmArg
{
 public:
  // type enum
  enum {
    Undef=0,

    Int,
    Double,
    CStr,
    AObject,
    APointer		// for passing pointers that are not owned by AmArg
  };

 private:
  // type
  short type;
    
  // value
  union {
    int         v_int;
    double      v_double;
    const char* v_cstr;
    ArgObject*  v_obj;
    void*	v_ptr;
  };

 public:
  AmArg(const AmArg& v)
    : type(v.type){
	
    switch(type){
    case Int: v_int = v.v_int; break;
    case Double: v_double = v.v_double; break;
    case CStr: v_cstr = strdup(v.v_cstr); break;
    case AObject: v_obj = v.v_obj; break;
    case APointer: v_ptr = v.v_ptr; break;
    default: assert(0);
    }
  }

  AmArg() 
    : type(Undef) 
    {}

  AmArg(const int& v)
    : type(Int),
    v_int(v)
    {}

  AmArg(const double& v)
    : type(Double),
    v_double(v)
    {}

  AmArg(const char* v)
    : type(CStr)
    {
      v_cstr = strdup(v);
    }

  AmArg(void* v)
    : type(APointer),
    v_ptr(v)
    { }

  ~AmArg() {
    if(type == CStr) free((void*)v_cstr);
  }

  short getType() const { return type; }

  void setBorrowedPointer(ArgObject* v) {
    type = AObject;
    v_obj = v;
  }
    

  int         asInt()    const { return v_int; }
  double      asDouble() const { return v_double; }
  const char* asCStr()   const { return v_cstr; }
  ArgObject*  asObject() const { return v_obj; }
  void*       asPointer() const { return v_ptr; }
};

/** \brief array of variable args for DI APIs*/
class AmArgArray
{
  vector<AmArg> v;

 public:
  struct OutOfBoundsException {
    OutOfBoundsException() { }
  };

  AmArgArray() : v() {}
  AmArgArray(const AmArgArray& a) : v(a.v) {}
    
  void push(const AmArg& a){
    v.push_back(a);
  }

  const AmArg& get(size_t idx) const {
	
    if (idx >= v.size())
      throw OutOfBoundsException();
    //	assert(idx < v.size());
    return v[idx];
  }

  size_t size() { return v.size(); }
};

#endif
