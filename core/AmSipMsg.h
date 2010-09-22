#ifndef __AMSIPMSG_H__
#define __AMSIPMSG_H__

#include <string>
using std::string;

#include "sip/trans_layer.h"

/* enforce common naming in Req&Rpl */
class _AmSipMsgInDlg
{
 public:
  string from;
  string from_uri;
  string from_tag;

  string to;
  string to_tag;

  string callid;
  unsigned int cseq;
  string cseq_method;

  unsigned int rseq;

  string route;
  string contact;

  string content_type;
  string hdrs;

  string body;

  // transaction ticket from sip stack
  trans_ticket tt;

  _AmSipMsgInDlg() : cseq(0), rseq(0) { }
  virtual ~_AmSipMsgInDlg() { };

  virtual string print() const = 0;
};

/** \brief represents a SIP reply */
class AmSipReply : public _AmSipMsgInDlg
{
 public:
  unsigned int code;
  string       reason;

 AmSipReply() : code(0), _AmSipMsgInDlg() { }
  ~AmSipReply() { }
  string print() const;
};


/** \brief represents a SIP request */
class AmSipRequest : public _AmSipMsgInDlg
{
 public:
  string method;

  string user;
  string domain;
  string r_uri;

  AmSipRequest() : _AmSipMsgInDlg() { }
  ~AmSipRequest() { }
  
  string print() const;
};

string getHeader(const string& hdrs,const string& hdr_name, bool single = false);

string getHeader(const string& hdrs,const string& hdr_name, 
		 const string& compact_hdr_name, bool single = false);

/** find a header, starting from char skip
    if found, value is between pos1 and pos2 
    and hdr start is the start of the header 
    @return true if found */
bool findHeader(const string& hdrs,const string& hdr_name, const size_t skip, 
		size_t& pos1, size_t& pos2, 
		size_t& hdr_start);

bool removeHeader(string& hdrs, const string& hdr_name);
#endif /* __AMSIPMSG_H__ */


/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 2
 * End:
 */
