/*
    Copyright (C) Anton Zagorskiy amberovsky@gmail.com
    Oyster-Telecom Laboratory
    
    Published under BSD License
*/

#ifndef _MWI_H
#define _MWI_H

#include "AmApi.h"
#include <string>

class MWI : public AmDynInvokeFactory, public AmDynInvoke
{
private:
    static MWI* _instance;
    static AmDynInvoke* MessageStorage;
    
    string presence_server;
    
    typedef struct
    {
	unsigned int new_msgs;
	unsigned int saved_msgs;
    } msg_info_struct;
    
    void getMsgInfo (const string& name, const string& domain, msg_info_struct& msg_info);
    void publish (const string& name, const string& domain);

public:
    MWI(const string& name);
    ~MWI();

    AmDynInvoke* getInstance(){ return _instance; }

    int onLoad();
    void invoke(const string& method, const AmArg& args, AmArg& ret);
};

#endif
