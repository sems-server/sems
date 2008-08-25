#ifndef __AMSIPMSG_H__
#define __AMSIPMSG_H__

#include <string>
using std::string;

/* enforce common naming in Req&Rpl */
class _AmSipMsgInDlg
{
  public:
    string       method;
    string       next_hop;
    string       route;

    string       contact;
    string       content_type;

    string       hdrs;
    string       body;
    unsigned int cseq;
    string       callid;

    string dstip; // IP where Ser received the message
    string port;  // Ser's SIP port

    string       serKey;

    _AmSipMsgInDlg() : cseq(0) { }
    virtual ~_AmSipMsgInDlg() { };

    virtual string print() = 0;
};

/** \brief represents a SIP reply */
class AmSipReply : public _AmSipMsgInDlg
{
 public:
  unsigned int code;
  string       reason;
  string       next_request_uri;

  /*TODO: this should be merged with request's from_/to_tag and moved above*/
  string       remote_tag;
  string       local_tag;


  AmSipReply() : code(0), _AmSipMsgInDlg() { }
  ~AmSipReply() { }
  string print();
};


/** \brief represents a SIP request */
class AmSipRequest : public _AmSipMsgInDlg
{
 public:
  string cmd;

  string user;
  string domain;
  string r_uri;
  string from_uri;
  string from;
  string to;
  string from_tag;
  string to_tag;

  AmSipRequest() : _AmSipMsgInDlg() { }
  ~AmSipRequest() { }
  
  string print();
};

string getHeader(const string& hdrs,const string& hdr_name);

string getHeader(const string& hdrs,const string& hdr_name, 
		 const string& compact_hdr_name);

/** find a header, 
    if found, value is between pos1 and pos2 
    and hdr start is the start of the header 
    @return true if found */
bool findHeader(const string& hdrs,const string& hdr_name, 
		size_t& pos1, size_t& pos2, 
		size_t& hdr_start);


#endif /* __AMSIPMSG_H__ */
