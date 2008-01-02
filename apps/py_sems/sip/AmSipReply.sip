
struct AmSipReply
{
private:
%TypeHeaderCode
#include "../../../core/AmSipMsg.h"
%End


public:

    unsigned int code;
    string       reason;
    string       next_request_uri;
    string       next_hop;
    string       route;
    string       hdrs;
    string       body;

    // Parsed from the hdrs
    // string       callid;
    string       remote_tag;
    string       local_tag;
    unsigned int cseq;

    AmSipReply();
};
