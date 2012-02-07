#ifndef _AmMimeBody_h_
#define _AmMimeBody_h_

#include <string>
#include <list>
#include <map>

using std::string;
using std::list;
using std::map;

class AmMimeBody
{
public:
  struct CTParam
  {

    enum Type {
      UNPARSED=0,
      BOUNDARY,
      OTHER
    };

    Type   type;
    string name;
    string value;

    CTParam(const string& name, const string& value)
      : type(UNPARSED), name(name), value(value) {}
  };

  typedef list<CTParam*>     CTParams;
  typedef list<AmMimeBody*>  Parts;

private:
  string   ct_type;
  string   ct_subtype;
  CTParams ct_params;
  CTParam* mp_boundary;

  string         hdrs;
  unsigned int   content_len;
  unsigned char* payload;
  
  Parts parts;

  void clearCTParams();
  void clearParts();
  void clearPayload();

  int parseCT(const string& ct);
  int parseCTParams(const char* c, const char* end);
  int parseCTParamType(CTParam* p);
  int parseMultipart(unsigned char* buf, unsigned int len);
  int findNextBoundary(unsigned char** beg, unsigned char** end);
  int parseSinglePart(unsigned char* buf, unsigned int len);

public:
  AmMimeBody();
  ~AmMimeBody();

  int  parse(const string& ct, unsigned char* buf, unsigned int len);

  void setCTType(const string& type);
  void setCTSubType(const string& subtype);
  void setPayload(unsigned char* buf, unsigned int len);
  void setHeaders(const string& hdrs);

  const string&   getCTType() const { return ct_type; }
  const string&   getCTSubType() const { return ct_subtype; }
  const CTParams& getCTParams() const { return ct_params; }
  const Parts&    getParts() const { return parts; }
};

#endif
