/*
  Copyright (C) Anton Zagorskiy amberovsky@gmail.com
  Oyster-Telecom Laboratory
        
  Published under BSD License
*/
            
#include "AmPlugIn.h"
#include "AmSession.h"
#include "AmConfigReader.h"
#include "AmUtils.h"
#include "log.h"
#include "mwi.h"
#include <string>

MWI* MWI::_instance = 0;
AmDynInvoke* MWI::MessageStorage = 0;

EXPORT_PLUGIN_CLASS_FACTORY(MWI, MOD_NAME);

MWI::MWI(const string& name)
  : AmDynInvokeFactory(name) { 
  _instance = this; 
};

MWI::~MWI() { };


int MWI::onLoad()
{
  AmDynInvokeFactory* ms_fact = 
    AmPlugIn::instance()->getFactory4Di("msg_storage");

  if(!ms_fact || !(MessageStorage = ms_fact->getInstance())) {
    ERROR("could not load msg_storage. Load a msg_storage implementation module.\n");
    return -1;
  };

  // register the publish method as event sink for msg_storage events
  AmArg es_args,ret;
  es_args.push(this);
  es_args.push("publish");
  MessageStorage->invoke("events_subscribe",es_args,ret);
  
  AmConfigReader cfg;
  if(cfg.loadFile(AmConfig::ModConfigPath + "mwi.conf")) {
    ERROR("can not load configuration file\n");
    return -1;
  };
  
  presence_server = cfg.getParameter("presence_server");
  if (presence_server.length())
    DBG("set presence server '%s'\n", presence_server.c_str());
  else {
    ERROR("parameter 'presence_server' did not found in the configuration file\n");
    return -1;
  }                
  
  DBG("MWI module loaded.\n");
  return 0;
};


void MWI::publish(const string& user, const string& domain)
{
  int new_msgs = 0;
  int all_msgs = 0;
  string headers, body;
                                            
  AmArg di_args, ret;
  di_args.push(domain.c_str());
  di_args.push(user.c_str());
    
  MessageStorage->invoke("userdir_open",di_args,ret);
    
  if (!ret.size() || !isArgInt(ret.get(0))) {
    ERROR("userdir_open for user '%s' domain '%s' returned no (valid) result.\n", user.c_str(), domain.c_str());
    return;
  };
    
  all_msgs = ret.get(1).size();
  for (size_t i = 0; i < ret.get(1).size(); i++) {
    AmArg& elem = ret.get(1).get(i);
    
    if (elem.get(2).asInt()) // skip empty messages
      new_msgs += elem.get(1).asInt();
    else
      all_msgs--;
  };
                                                  
  DBG("Found %d new and %d old messages\n", new_msgs, all_msgs - new_msgs);
  string vm_buf = int2str(new_msgs) + "/" + int2str(all_msgs - new_msgs);

  headers = "Event: message-summary\r\n";
  headers += "Subscription-State: active\r\n";
    
  if (new_msgs > 0)
    body = "Messages-Waiting: yes\r\n";
  else
    body = "Messages-Waiting: no\r\n";

  body += "Message-Account: sip:" + user + "@" + domain + "\r\n";
  body += "Voice-Message: " + vm_buf + " (" + vm_buf + ")\r\n";

  AmMimeBody sms_body;
  sms_body.addPart("application/simple-message-summary");
  sms_body.setPayload((const unsigned char*)body.c_str(),body.length());

  AmSipDialog tmp_d(NULL);
  tmp_d.local_party = string("<sip:mwi-publisher@") + presence_server + ">";
  tmp_d.remote_party = domain.c_str();
  tmp_d.route = "sip:" + presence_server;
  tmp_d.remote_uri = "sip:" + user + "@" + domain;
  tmp_d.callid = AmSession::getNewId() + "@" + presence_server;
  tmp_d.local_tag = AmSession::getNewId();
  tmp_d.sendRequest(SIP_METH_NOTIFY, &sms_body, headers);     
};

void MWI::invoke(const string& method, const AmArg& args, AmArg& ret)
{
  if (method == "publish") {
    string user, domain;
    user = args.get(1).asCStr();
    domain = args.get(0).asCStr();
    publish(user, domain);
    ret.push(0);
  }
  else
    throw AmDynInvoke::NotImplemented(method); 
};
