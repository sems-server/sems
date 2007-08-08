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

/** base for Objects as @see AmArg parameter, not owned by AmArg (!) */
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
    AObject,		// for passing pointers to objects not owned by AmArg
    
    Array
  };

  struct OutOfBoundsException {
    OutOfBoundsException() { }
  };

  struct TypeMismatchException {
    TypeMismatchException() { }
  };
  
  typedef std::vector<AmArg> ValueArray;

 private:
  // type
  short type;
    
  // value
  union {
    int            v_int;
    double         v_double;
    const char*    v_cstr;
    ArgObject*     v_obj;

    ValueArray*    v_array;
  };


  void assertArray();
  void assertArray() const;

  void invalidate();

 public:
  AmArg() 
    : type(Undef) 
    { }

  AmArg(const AmArg& v);
  
  AmArg(const int& v)
    : type(Int),
    v_int(v)
    { }

  AmArg(const double& v)
    : type(Double),
    v_double(v)
    { }

  AmArg(const char* v)
    : type(CStr)
    {
      v_cstr = strdup(v);
    }

  ~AmArg() { invalidate(); }

  short getType() const { return type; }

#define isArgArray(a) (AmArg::Array == a.getType())
#define isArgDouble(a) (AmArg::Array == a.getType())
#define isArgInt(a) (AmArg::Int == a.getType())
#define isArgCStr(a) (AmArg::CStr == a.getType())
#define isArgAObject(a) (AmArg::AObject == a.getType())

#define assertArgArray(a) \
  if (!isArgArray(a)) \
    throw AmArg::TypeMismatchException();
#define assertArgDouble(a) \
  if (!isArgDouble(a)) \
    throw AmArg::TypeMismatchException();
#define assertArgInt(a) \
  if (!isArgInt(a)) \
    throw AmArg::TypeMismatchException();
#define assertArgCStr(a) \
  if (!isArgCStr(a)) \
    throw AmArg::TypeMismatchException();
#define assertArgAObject(a) \
  if (!isArgAObject(a)) \
    throw AmArg::TypeMismatchException();
   

  void setBorrowedPointer(ArgObject* v) {
    type = AObject;
    v_obj = v;
  }

  int         asInt()    const { return v_int; }
  double      asDouble() const { return v_double; }
  const char* asCStr()   const { return v_cstr; }
  ArgObject*  asObject() const { return v_obj; }

  // operations on arrays
  void assertArray(size_t s);

  void push(const AmArg& a);
  
  const size_t size() const;

  /** throws OutOfBoundsException if array too small */
  AmArg& get(size_t idx);

  /** throws OutOfBoundsException if array too small */
  AmArg& get(size_t idx) const;

  /** resizes array if too small */
  AmArg& operator[](size_t idx);
};

#endif
