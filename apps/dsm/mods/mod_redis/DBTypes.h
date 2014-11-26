#ifndef _DBTypes_h_
#define _DBTypes_h_

#include "AmArg.h"
#include "AmConfigReader.h"

#include <map>
#include <list>
using std::map;
using std::list;

struct DBIdxType
{
  string key;
  string value;
  
  DBIdxType()  {}
  DBIdxType(const string& key, const string& value)
    : key(key), value(value)
  {}
};

typedef map<string,AmArg> DBMapType;
typedef list<DBIdxType>   DBIdxList;

#define DB_E_OK          0
#define DB_E_CONNECTION -1
#define DB_E_WRITE      -2
#define DB_E_READ       -3

enum RestoreResult {
  SUCCESS,
  FAILURE,   // an error
  NOT_FOUND  // object not found in the storage
};

#endif
