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

#include <string>
using std::string;

#include <map>

#include "log.h"

/** base for Objects as @see AmArg parameter, not owned by AmArg (!) */
class ArgObject {
 public:
  ArgObject() { }
  virtual ~ArgObject() { }
};

struct ArgBlob {
  char* data;
  int   len;
  
ArgBlob() 
: data(NULL),len(0)
  {  
  }

  ArgBlob(const ArgBlob& a) {
    len = a.len;
    data = (char*)malloc(len);
    if (data)
      memcpy(data, a.data, len);
  }
  
  ArgBlob(const char* _data, int _len) {
    len = _len;
    data = (char*)malloc(len);
    if (data)
      memcpy(data, _data, len);
  }
  
  ~ArgBlob() { if (data) free(data); }
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
    Blob,
    
    Array,
    Struct
  };

  struct OutOfBoundsException {
    OutOfBoundsException() { }
  };

  struct TypeMismatchException {
    TypeMismatchException() { }
  };
  
  typedef std::vector<AmArg> ValueArray;
  typedef std::map<std::string, AmArg> ValueStruct; 

 private:
  // type
  short type;
    
  // value
  union {
    int            v_int;
    double         v_double;
    const char*    v_cstr;
    ArgObject*     v_obj;
    ArgBlob*       v_blob;
    ValueArray*    v_array;
    ValueStruct*   v_struct;
  };


  void assertArray();
  void assertArray() const;

  void assertStruct();
  void assertStruct() const;

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
  
 AmArg(const ArgBlob v)
   : type(Blob)
  {
    v_blob = new ArgBlob(v);
  }

  // convenience constructors
  AmArg(vector<std::string>& v);
  AmArg(const vector<int>& v );
  AmArg(const vector<double>& v);
  AmArg(std::map<std::string, std::string>& v);
  AmArg(std::map<std::string, AmArg>& v);
  
  ~AmArg() { invalidate(); }

  short getType() const { return type; }

  AmArg& operator=(const AmArg& rhs);

#define isArgArray(a) (AmArg::Array == a.getType())
#define isArgStruct(a)(AmArg::Struct == a.getType())
#define isArgDouble(a) (AmArg::Array == a.getType())
#define isArgInt(a) (AmArg::Int == a.getType())
#define isArgCStr(a) (AmArg::CStr == a.getType())
#define isArgAObject(a) (AmArg::AObject == a.getType())
#define isArgBlob(a) (AmArg::Blob == a.getType())

#define assertArgArray(a)			\
  if (!isArgArray(a))				\
    throw AmArg::TypeMismatchException();
#define assertArgDouble(a)			\
  if (!isArgDouble(a))				\
    throw AmArg::TypeMismatchException();
#define assertArgInt(a)				\
  if (!isArgInt(a))				\
    throw AmArg::TypeMismatchException();
#define assertArgCStr(a)			\
  if (!isArgCStr(a))				\
    throw AmArg::TypeMismatchException();
#define assertArgAObject(a)			\
  if (!isArgAObject(a))				\
    throw AmArg::TypeMismatchException();   
#define assertArgBlob(a)			\
  if (!isArgBlob(a))				\
    throw AmArg::TypeMismatchException();
#define assertArgStruct(a)			\
  if (!isArgStruct(a))				\
    throw AmArg::TypeMismatchException();

  void setBorrowedPointer(ArgObject* v) {
    type = AObject;
    v_obj = v;
  }

  int         asInt()    const { return v_int; }
  double      asDouble() const { return v_double; }
  const char* asCStr()   const { return v_cstr; }
  ArgObject*  asObject() const { return v_obj; }
  ArgBlob*    asBlob()   const { return v_blob; }

  vector<string>     asStringVector()    const; 
  vector<int>        asIntVector()       const; 
  vector<double>     asDoubleVector()    const; 
  vector<ArgObject*> asArgObjectVector() const; 
  vector<ArgBlob>    asArgBlobVector()   const; 

  // operations on arrays
  void assertArray(size_t s);

  void push(const AmArg& a);

  void concat(const AmArg& a);
  
  const size_t size() const;

  /** throws OutOfBoundsException if array too small */
  AmArg& get(size_t idx);

  /** throws OutOfBoundsException if array too small */
  AmArg& get(size_t idx) const;

  /** resizes array if too small */
  AmArg& operator[](size_t idx);
  /** throws OutOfBoundsException if array too small */
  AmArg& operator[](size_t idx) const;

  /** resizes array if too small */
  AmArg& operator[](int idx);
  /** throws OutOfBoundsException if array too small */
  AmArg& operator[](int idx) const;

  AmArg& operator[](std::string key);
  AmArg& operator[](std::string key) const;
  AmArg& operator[](const char* key);
  AmArg& operator[](const char* key) const;

  /** Check for the existence of a struct member by name. */
  bool hasMember(const std::string& name) const;
  bool hasMember(const char* name) const;

  std::vector<std::string> enumerateKeys() const;
  ValueStruct::const_iterator begin() const;
  ValueStruct::const_iterator end() const;

  /** 
   * throws exception if arg array does not conform to spec 
   *   i  - int 
   *   f  - double
   *   s  - cstr
   *   o  - object
   *   b  - blob
   *   a  - array
   *   u  - struct
   *
   *   e.g. "ssif" -> [cstr, cstr, int, double]
   */
  void assertArrayFmt(const char* format) const;
};

#endif

