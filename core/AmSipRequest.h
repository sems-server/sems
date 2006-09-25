#ifndef AmSipRequest_h
#define AmSipRequest_h

#include "AmEventQueue.h"

#include <string>
using std::string;

/** \brief represents a SIP request */
class AmSipRequest
{
public:
    string cmd;

    string method;
    string user;
    string domain;
    string dstip; // IP where Ser received the message
    string port;  // Ser's SIP port
    string r_uri;
    string from_uri;
    string from;
    string to;
    string callid;
    string from_tag;
    string to_tag;

    unsigned int cseq;

    string hdrs;
    string body;

    string route;     // record routing
    string next_hop;  // next_hop for t_uac_dlg
    
    string key; // transaction key to be used in t_reply
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

#endif
