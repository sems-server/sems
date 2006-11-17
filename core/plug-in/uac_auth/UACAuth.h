#ifndef UACAuth_h
#define UACAuth_h

#include "AmApi.h"
#include "AmSession.h"

#include <string>
using std::string;
#include <map>
using std::map;

#define HASHLEN 16
typedef unsigned char HASH[HASHLEN];

#define HASHHEXLEN 32
typedef unsigned char HASHHEX[HASHHEXLEN+1];

struct UACAuthDigestChallenge {
  std::string domain;
  std::string realm;
  std::string qop;

  std::string nonce;
  std::string opaque;
  bool stale;
  std::string algorithm;
};

struct UACAuthCred {
	string realm;
	string user;
	string pwd;
	UACAuthCred(const string& realm,
							 const string& user,
							 const string& pwd)
		: realm(realm), user(user), pwd(pwd) { }
};

class CredentialHolder {
	UACAuthCred cred;
 public:
	CredentialHolder(const string& realm,
					 const string& user,
					 const string& pwd)
		: cred(realm,user,pwd) { }
	UACAuthCred* getCredentials() { return &cred; }
};

class UACAuthFactory
: public AmSessionEventHandlerFactory
{
public:
    UACAuthFactory(const string& name)
	: AmSessionEventHandlerFactory(name)
		{ }
	
    int onLoad();

	// SessionEventHandler API
    AmSessionEventHandler* getHandler(AmSession* s);
    bool onInvite(const AmSipRequest&);
};

struct SIPRequestInfo {
	string method;
	string content_type;
	string body;
	string hdrs;
	
	SIPRequestInfo(const string& method, 
				   const string& content_type,
				   const string& body,
				   const string& hdrs)
		: method(method), content_type(content_type),
		 body(body), hdrs(hdrs) { }

	SIPRequestInfo() {}

};

class UACAuth: public AmSessionEventHandler
{
	map<unsigned int, SIPRequestInfo> sent_requests;

	UACAuthCred* credential;
	
	std::string find_attribute(const std::string& name, const std::string& header);
	bool parse_header(const std::string& auth_hdr, UACAuthDigestChallenge& challenge);

	void uac_calc_HA1(UACAuthDigestChallenge& challenge,
					std::string cnonce,
					  HASHHEX sess_key);

	void uac_calc_HA2( const std::string& method, const std::string& uri,
					   UACAuthDigestChallenge& challenge,
					   HASHHEX hentity,
					   HASHHEX HA2Hex );
	
	void uac_calc_response( HASHHEX ha1, HASHHEX ha2,
							UACAuthDigestChallenge& challenge,
							const std::string& nc, const std::string& cnonce,
							HASHHEX response);
	
	/** 
	 *  do auth on cmd with nonce in auth_hdr if possible 
	 *  @return true if successful 
	 */
	bool do_auth(const unsigned int code, const string& auth_hdr,  
					  const string& method, const string& uri, string& result);
	
 public:
	
	UACAuth(AmSession* s, UACAuthCred* cred);
	virtual ~UACAuth(){ }
  
	/* SEH Hooks @see AmSessionEventHandler */
	virtual bool process(AmEvent*);
	virtual bool onSipEvent(AmSipEvent*);
	virtual bool onSipRequest(const AmSipRequest&);
	virtual bool onSipReply(const AmSipReply&);
	
	virtual bool onSendRequest(const string& method, 
							   const string& content_type,
							   const string& body,
							   string& hdrs,
							   unsigned int cseq);
	
	virtual bool onSendReply(const AmSipRequest& req,
							 unsigned int  code,
							 const string& reason,
							 const string& content_type,
							 const string& body,
							 string& hdrs);
};


#endif
