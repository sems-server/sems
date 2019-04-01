#ifndef __AMSIPMSG_H__
#define __AMSIPMSG_H__
#include "AmArg.h"
#include "AmMimeBody.h"

#include <string>
using std::string;

#include "sip/trans_layer.h"

/* enforce common naming in Req&Rpl */
class _AmSipMsgInDlg
  : public AmObject
{
 public:
  string from;
  string from_tag;

  string to;
  string to_tag;

  string callid;

  unsigned int cseq;
  string cseq_method;

  unsigned int rseq;

  string route;
  string contact;

  string hdrs;

  AmMimeBody body;

  // transaction ticket from sip stack
  trans_ticket tt;

  string         remote_ip;
  unsigned short remote_port;
  string         local_ip;
  unsigned short local_port;
  string         trsp;

  _AmSipMsgInDlg() : cseq(0), rseq(0) { }
  virtual ~_AmSipMsgInDlg() { };

  virtual string print() const = 0;
};

#ifdef PROPAGATE_UNPARSED_REPLY_HEADERS

struct AmSipHeader
{
  string name, value;
  AmSipHeader() { }
  AmSipHeader(const string &_name, const string &_value): name(_name), value(_value) { }
  AmSipHeader(const cstring &_name, const cstring &_value): name(_name.s, _name.len), value(_value.s, _value.len) { }
};

#endif

/** \brief represents a SIP reply */
class AmSipReply : public _AmSipMsgInDlg
{
 public:
  unsigned int code;
  string       reason;
  string       to_uri;
#ifdef PROPAGATE_UNPARSED_REPLY_HEADERS
  list<AmSipHeader> unparsed_headers;
#endif

 AmSipReply() : _AmSipMsgInDlg(), code(0) { }
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
  string from_uri;

  string rack_method;
  unsigned int rack_cseq;

  string vias;
  string via1;
  string via_branch;
  bool   first_hop;

  int max_forwards;

  unsigned short local_if;

  AmSipRequest();
  ~AmSipRequest() { }
  
  string print() const;
  void log(msg_logger *logger) const;
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

/** @return whether header hdr_name is in hdrs */
bool hasHeader(const string& hdrs,const string& hdr_name);

bool removeHeader(string& hdrs, const string& hdr_name);

/** add an option tag @param tag to list @param hdr_name */
void addOptionTag(string& hdrs, const string& hdr_name, const string& tag);

/** remove an option tag @param tag from list @param hdr_name */
void removeOptionTag(string& hdrs, const string& hdr_name, const string& tag);

#endif /* __AMSIPMSG_H__ */


/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 2
 * End:
 */
