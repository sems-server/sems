
class AmSipRequest
{

%TypeHeaderCode
#include "../../../core/AmSipMsg.h"
%End

public:
    string cmd;

    string method;
    string user;
    string domain;
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
};

string getHeader(const string& /* hdrs */,const string& /* hdr_name */);
string getHeader(const string& /*hdrs*/, const string& /*hdr_name*/, const string& /*compact_hdr_name*/);
