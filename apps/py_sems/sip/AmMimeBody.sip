


struct AmContentType
{

%TypeHeaderCode
#include "../../../core/AmMimeBody.h"
%End

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

    int parseType();
  };

  typedef list<Param*> Params;

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

%TypeHeaderCode
#include "../../../core/AmMimeBody.h"
%End

public:

  typedef list<AmMimeBody*>  Parts;

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
  string getCTStr() const;

  /** Get content-type with parameters */
  string getCTHdr() const;
  
  /** @return the list of sub-parts */
  const Parts& getParts() const;

  /** @return the sub-part headers */
  const string& getHeaders();

  /**
   * @return a pointer to the payload of this part.
   *         in case of multi-part, NULL is returned.
   */
  const unsigned char* getPayload();

  /**
   * @return the payload length of this part.
   *         in case of multi-part, 0 is returned.
   */
  unsigned int   getLen();

  /** @return true if no payload assigned and no sub-parts available */
  bool empty() const;

  /** @return true if this part has the provided content-type */
  bool isContentType(const string& /* content_type */) const;

  /** 
   * @return a pointer to a part of the coresponding 
   *         content-type (if available).
   *         This could be a pointer to this body.
   */
  AmMimeBody* hasContentType(const string& /*content_type*/);

  /** 
   * @return a const pointer to a part of the coresponding 
   *         content-type (if available).
   *         This could be a pointer to this body.
   */
  const AmMimeBody* hasContentType(const string& /* content_type*/ ) const;

  /**
   * Print the body including sub-parts suitable for sending
   * within the body of a SIP message.
   */
  void print(string& /* buf */) const;

  const AmContentType &getContentType();
  void setContentType(const AmContentType & /*_ct*/);
  void addPart(const AmMimeBody & /*part*/);

};
