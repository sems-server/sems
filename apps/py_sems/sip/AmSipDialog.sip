
class AmSipDialog
{

%TypeHeaderCode
#include "../../../core/AmSipDialog.h"
%End


public:
    enum Status {
    Disconnected=0,
    Trying,
    Proceeding,
    Cancelling,
    Early,
    Connected,
    Disconnecting
    };

    string user;         // local user
    string domain;       // local domain

    string local_uri;    // local uri
    string remote_uri;   // remote uri

    string contact_uri;  // pre-calculated contact uri

    string getCallid();
    string getRemoteTag();
    string getLocalTag();

    string getRemoteParty(); // To/From
    string getLocalParty();  // To/From

    int cseq;            // CSeq for next request
    int r_cseq;

    string route; // record routing

    bool   getUACTransPending();
    int    getStatus();
    string getContactHdr();

    void onRxRequest(const AmSipRequest& /* req */);
    void onRxReply(const AmSipReply& /* reply */);

    int reply(const AmSipRequest& /* req */,
 	      unsigned int  /* code */, 
 	      const string& /* reason */,
 	      const string& /* content_type */,
 	      const AmMimeBody* /* body */,
 	      const string& /* hdrs */);

    int sendRequest(const string& /* method */, 
		    const string& /* content_type */,
		    const AmMimeBody* /* body */,
		    const string& /* hdrs */);
    
    int bye();

    int cancel();

    int update(const AmMimeBody* /* body */,
	       const string& /* hdrs */);

    int reinvite(const AmMimeBody* /* body */,
    	         const string& /* hdrs */);
};
