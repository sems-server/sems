#ifndef _AmMimeBody_h_
#define _AmMimeBody_h_

#include <string>
#include <list>
#include <map>

using std::string;
using std::list;
using std::map;

struct AmContentType
{
  struct Param
  {

    enum Type {
      UNPARSED=0,
      BOUNDARY,
      OTHER
    };

    Type   type;
    string name;
    string value;

    Param(const string& name, const string& value)
      : type(UNPARSED), name(name), value(value) {}

    int parseType();
  };

  typedef list<Param*> Params;

  string type;
  string subtype;
  Params params;
  Param* mp_boundary;

  AmContentType();
  AmContentType(const AmContentType& ct);
  ~AmContentType();

  const AmContentType& operator = (const AmContentType& r_ct);

  int  parse(const string& ct);
  int  parseParams(const char* c, const char* end);

  void setType(const string& t);
  void setSubType(const string& st);

  bool isType(const string& t) const;
  bool isSubType(const string& st) const;
  bool hasContentType(const string& content_type) const;

  /** get content-type without any parameters */
  string getStr() const;

  /** get content-type with parameters */
  string getHdr() const;

  /** Clear and free param list */
  void clearParams();
};

class AmMimeBody
{
public:
  typedef list<AmMimeBody*>  Parts;

private:
  AmContentType  ct;
  string         hdrs;
  unsigned int   content_len;
  unsigned char* payload;
  
  Parts parts;

  void clearParts();
  void clearPayload();

  int parseMultipart(const unsigned char* buf, unsigned int len);
  int findNextBoundary(unsigned char** beg, unsigned char** end);
  int parseSinglePart(unsigned char* buf, unsigned int len);

  void convertToMultipart();

public:
  /** Empty constructor */
  AmMimeBody();

  /** Deep-copy constructor */
  AmMimeBody(const AmMimeBody& body);

  /** Destuctor */
  ~AmMimeBody();

  /** Deep copy operator */
  const AmMimeBody& operator = (const AmMimeBody& r_body);

  /** Parse a body (single & multi-part) */
  int  parse(const string& content_type, 
	     const unsigned char* buf, 
	     unsigned int len);

  /** Set the payload of this body */
  void setPayload(const unsigned char* buf, unsigned int len);

  /** Set part headers (intended for sub-parts)*/
  void setHeaders(const string& hdrs);

  /** 
   * Add a new part to this body, possibly
   * converting to multi-part if necessary.
   * @return a pointer to the new empty part.
   */
  AmMimeBody* addPart(const string& content_type);

  /** Get content-type without any parameters */
  string getCTStr() const { return ct.getStr(); }

  /** Get content-type with parameters */
  string getCTHdr() const { return ct.getHdr(); }
  
  /** @return the list of sub-parts */
  const Parts& getParts() const { return parts; }

  /** @return the sub-part headers */
  const string& getHeaders() const { return hdrs; }

  /**
   * @return a pointer to the payload of this part.
   *         in case of multi-part, NULL is returned.
   */
  const unsigned char* getPayload() const { return payload; }

  /**
   * @return the payload length of this part.
   *         in case of multi-part, 0 is returned.
   */
  unsigned int   getLen() const { return content_len; }

  /** @return true if no payload assigned and no sub-parts available */
  bool empty() const;

  /** @return true if this part has the provided content-type */
  bool isContentType(const string& content_type) const;

  /** 
   * @return a pointer to a part of the coresponding 
   *         content-type (if available).
   *         This could be a pointer to this body.
   */
  AmMimeBody* hasContentType(const string& content_type);

  /** 
   * @return a const pointer to a part of the coresponding 
   *         content-type (if available).
   *         This could be a pointer to this body.
   */
  const AmMimeBody* hasContentType(const string& content_type) const;

  /**
   * Print the body including sub-parts suitable for sending
   * within the body of a SIP message.
   */
  void print(string& buf) const;
};

#endif
