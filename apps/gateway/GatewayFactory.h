#ifndef _ISDNGATEWAYFACTORY_H_
#define _ISDNGATEWAYFACTORY_H_

#include "GWSession.h"
#include "AmConfigReader.h"
#include <string>
using std::string;

class GatewayFactory: public AmSessionFactory
{
 public:
    GatewayFactory(const string& _app_name);
    ~GatewayFactory();
    //Auth api
    AmSessionEventHandlerFactory* uac_auth_f;

    int onLoad();
    AmSession* onInvite(const AmSipRequest& req);
    AmSession* onInvite(const AmSipRequest& req, AmArg& session_params);
 private:
    static GatewayFactory* _instance;
    bool	auth_enable;
    std::string auth_realm;
    std::string auth_user;
    std::string auth_pwd;
                                 
};

extern AmConfigReader gwconf;
#endif

