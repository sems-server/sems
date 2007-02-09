
class AmSipRequest
{

%TypeHeaderCode
#include "../../../core/AmSipRequest.h"
%End

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

string getHeader(const string& /* hdrs */,const string& /* hdr_name */);
