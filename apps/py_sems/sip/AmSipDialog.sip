
class AmSipDialog
{

%TypeHeaderCode
#include "../../../core/AmSipDialog.h"
%End


public:
    enum Status {
	
	Disconnected=0,
	Pending,
	Connected,
	Disconnecting
    };

    string user;         // local user
    string domain;       // local domain
    string sip_ip;       // destination IP of first received message
    string sip_port;     // optional: SIP port

    string local_uri;    // local uri
    string remote_uri;   // remote uri

    string contact_uri;  // pre-calculated contact uri

    string callid;
    string remote_tag;
    string local_tag;

    string remote_party; // To/From
    string local_party;  // To/From

    string next_hop;     // next_hop for t_uac_dlg

    int cseq;            // CSeq for next request

    string getRoute(); // record routing
    void   setRoute(const string& /* n_route */);

    bool   getUACTransPending();
    int    getStatus();
    string getContactHdr();

    void updateStatus(const AmSipRequest& /* req */);
    void updateStatus(const AmSipReply& /* reply */);

    int reply(const AmSipRequest& /* req */,
 	      unsigned int  /* code */, 
 	      const string& /* reason */,
 	      const string& /* content_type */,
 	      const string& /* body */,
 	      const string& /* hdrs */);

    int sendRequest(const string& /* method */, 
		    const string& /* content_type */,
		    const string& /* body */,
		    const string& /* hdrs */);
    
    int bye();

    int cancel();

    int update(const string& /* hdrs */);

    int reinvite(const string& /* hdrs */,
		 const string& /* content_type */,
		 const string& /* body */);
};
