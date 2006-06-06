#ifndef AmSipDialog_h
#define AmSipDialog_h

#include "AmSipRequest.h"
#include "AmSipReply.h"

#include <string>
#include <vector>
#include <map>
using std::string;
using std::vector;
using std::map;

#define CONTACT_USER_PREFIX "sems"


struct AmSipTransaction
{
    string       method;
    unsigned int cseq;

    // last reply code
    // (sent or received)
    //int reply_code;

    AmSipTransaction(const string& method, unsigned int cseq)
	: method(method),
	  cseq(cseq)
    {}

    AmSipTransaction()
    {}
};

typedef map<int,AmSipTransaction> TransMap;

class AmSipDialogEventHandler 
{
public:
    virtual void onSendRequest(const string& method,
			       const string& content_type,
			       const string& body,
			       string& hdrs)=0;

    virtual void onSendReply(const AmSipRequest& req,
			     unsigned int  code,
			     const string& reason,
			     const string& content_type,
			     const string& body,
			     string& hdrs)=0;
};


class AmSipDialog
{
    int status;

    TransMap uas_trans;
    TransMap uac_trans;

    AmSipDialogEventHandler* hdl;
    vector<string> route;        // record routing

    static int send_reply(const string& msg, 
			  const string& reply_sock);

    int updateStatusReply(const AmSipRequest& req, 
			  unsigned int code);

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

    string getRoute(); // record routing
    void   setRoute(const string& n_route);

    string next_hop;     // next_hop for t_uac_dlg

    int cseq;            // CSeq for next request

    AmSipDialog(AmSipDialogEventHandler* h=0)
	: status(Disconnected),cseq(10),hdl(h)
    {}

    ~AmSipDialog();

    bool   getUACTransPending() { return !uac_trans.empty(); }
    int    getStatus() { return status; }
    string getContactHdr();

    void updateStatus(const AmSipRequest& req);
    void updateStatus(const AmSipReply& reply);

    int reply(const AmSipRequest& req, // Ser's transaction key
	      unsigned int  code, 
	      const string& reason,
	      const string& content_type = "",
	      const string& body = "",
	      const string& hdrs = "");

    int sendRequest(const string& method, 
		    const string& content_type = "",
		    const string& body = "",
		    const string& hdrs = "");
    
    int bye();
    int cancel();
    int update(const string& hdrs);
    int reinvite(const string& hdrs,  
		 const string& content_type,
		 const string& body);

    /**
     * @return true if a transaction could be found that
     *              matches the CANCEL's one.
     */
    bool match_cancel(const AmSipRequest& cancel_req);

    static int reply_error(const AmSipRequest& req,
			   unsigned int  code, 
			   const string& reason,
			   const string& hdrs = "");
};


#endif
