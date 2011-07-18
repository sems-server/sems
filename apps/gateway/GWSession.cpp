#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "globals.h"
#include "GWSession.h"
#include "AmMediaProcessor.h"
#include "AmApi.h"
#include "AmUAC.h"
#include "AmSession.h"
#include "AmSessionContainer.h"
#include "ampi/UACAuthAPI.h"
#include "GatewayFactory.h"
#include "log.h"
#include"mISDNChannel.h"

GWSession::GWSession(const string& auth_realm, const string& auth_user, const string& auth_pwd) : 
						      credentials(auth_realm, auth_user, auth_pwd) {
	DBG("new GWSession@%p\n", this);
}
UACAuthCred* GWSession::getCredentials() {
        return &credentials;
}

void GWSession::setOtherLeg(AmAudio *otherleg) {
	m_OtherLeg=otherleg;
}
void GWSession::onInvite(const AmSipRequest& req) {
    DBG("GWSession::onInvite\n");
//    RTPStream()->setMonitorRTPTimeout(false);
    invite_req=req;
    return;
}

void GWSession::onSessionStart(const AmSipRequest& req) {
        DBG("GWSession::onSessionStart\n");
        try {
            string sdp_reply;
            acceptAudio(req.body,req.hdrs,&sdp_reply);
            if(dlg.reply(req,200,"OK Isdn side state is: CONNECTED", "application/sdp",sdp_reply) != 0)
                     throw AmSession::Exception(500,"could not send response");
	}catch(const AmSession::Exception& e){
    	    ERROR("%i %s\n",e.code,e.reason.c_str());
            setStopped();
            dlg.reply(req,e.code,e.reason);
            return;
        }
        DBG("GWSession::onSessionStart Setting Audio\n");
	setInOut((AmAudio *)(m_OtherLeg),(AmAudio *)(m_OtherLeg));
	AmSession::onSessionStart(req);
	AmMediaProcessor::instance()->addSession(this, callgroup);
}	
void GWSession::onSessionStart(const AmSipReply& reply) {
        DBG("GWSession::onSessionStart(reply)\n");
	DBG("calling ((mISDNChannel*)m_otherleg)->accept();\n");
        int ret=((mISDNChannel*)m_OtherLeg)->accept();
        DBG("GWSession::onSessionStart Setting Audio\n");
	setInOut((AmAudio *)(m_OtherLeg),(AmAudio *)(m_OtherLeg)); 
        AmSession::onSessionStart(reply);

}
		    
void GWSession::onBye(const AmSipRequest& req) {
	DBG("GWSession::onBye\n");
	int ret=((mISDNChannel*)m_OtherLeg)->hangup();
        AmSession::onBye(req);
		
}
void GWSession::onCancel(const AmSipRequest& req) {
	DBG("GWSession::onCancel\n");
	int ret=((mISDNChannel*)m_OtherLeg)->hangup();
        AmSession::onCancel(req);
		
}

GWSession::~GWSession()
{
	INFO("destroying GWSession!\n");
}

// we just need a hack this function for INVITE as orginal executes onSessionStart imediately after OnInvite
void GWSession::onSipRequest(const AmSipRequest& req)
{
    DBG("GWSession::onSipRequest check 1\n");
    if(req.method == "INVITE"){
	dlg.updateStatus(req);
	onInvite(req);
  } else {
    DBG("GWSession::onSipRequest calling parent\n");
    AmSession::onSipRequest(req);
  }
}

void GWSession::onSipReply(const AmSipReply& reply, AmSipDialog::Status old_dlg_status) {
    int status = dlg.getStatus();
    DBG("GWSession::onSipReply: code = %i, reason = %s\n, status = %i\n",  
	reply.code,reply.reason.c_str(),dlg.getStatus());
    if((dlg.getStatus()==AmSipDialog::Pending)&&(reply.code==183)) {	onProgress(reply);   }
    if((dlg.getStatus()==AmSipDialog::Pending)&&(reply.code>=300)) {	
	int ret=((mISDNChannel*)m_OtherLeg)->hangup();
    }
    DBG("GWSession::onSipReply calling parent\n");
    AmSession::onSipReply(reply, old_dlg_status);
}

void GWSession::on_stop() {
    DBG("GWSession::on_stop\n");
    if (!getDetached())
    	AmMediaProcessor::instance()->clearSession(this);
    else
        clearAudio();
}
void GWSession::onRinging(const AmSipReply& reply) {
    DBG("GWSession::onRinging\n");
    //TODO
}
void GWSession::onProgress(const AmSipReply& reply){
    DBG("GWSession::onProgress\n");
    //TODO
}

GWSession* GWSession::CallFromOutside(std::string &fromnumber, std::string &tonumber, int backend, AmAudio* chan) {
    AmArg* c_args = new AmArg();
    std::string user=gwconf.getParameter("auth_user","");
    std::string r_uri="sip:@";
    r_uri.insert(4,tonumber);r_uri.append(gwconf.getParameter("calleddomain",""));
    std::string from="sip:@";
    from.insert(4,fromnumber);from.append(gwconf.getParameter("callerdomain",""));
    std::string from_uri="sip:@";
    from_uri.insert(4,fromnumber);from_uri.append(gwconf.getParameter("callerdomain",""));
    std::string to="sip:@";
    to.insert(4,tonumber);to.append(gwconf.getParameter("calleddomain",""));
    DBG ("GWSession::CallFromOutside user=%s r_uri=%s from=%s to=%s\n",user.c_str(),r_uri.c_str(),from.c_str(),to.c_str()); 
    AmSession* s = AmUAC::dialout(user, 	//user
	"gateway",                              //app_name
	r_uri,              //r_uri
	from,            //from
	from_uri ,             //from_uri
	to,              //to
	string(""), // local_tag (callid)
	string(""), // headers
	c_args);
    DBG("GWCall::CallFromOutside session=%p\n",s);
    //this is static function so we dont that 'this' pointer yet as GWcall object is created in factory::onInvite. so we use pointer returned by dialout
    ((GWSession*)s)->setOtherLeg(chan);
    //for early media?
//    ((GWSession*)s)->setInOut(chan,chan);
    return (GWSession *)s;
}

/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
