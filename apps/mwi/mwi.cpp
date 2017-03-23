/*
  Copyright (C) Anton Zagorskiy amberovsky@gmail.com
  Oyster-Telecom Laboratory

  Published under BSD License
*/

#include "AmPlugIn.h"
#include "AmSession.h"
#include "AmConfig.h"
#include "AmConfigReader.h"
#include "AmUtils.h"
#include "log.h"
#include "mwi.h"
#include <string>

MWI* MWI::_instance = 0;
AmDynInvoke* MWI::MessageStorage = 0;

EXPORT_PLUGIN_CLASS_FACTORY(MWI, MOD_NAME);

MWI::MWI(const string& name) : AmDynInvokeFactory(name)
{
  _instance = this;
};

MWI::~MWI() { };


int MWI::onLoad()
{
  AmDynInvokeFactory* ms_fact =
    AmPlugIn::instance()->getFactory4Di("msg_storage");

  if (!ms_fact || !(MessageStorage = ms_fact->getInstance())) {
    ERROR("could not load msg_storage. Load a msg_storage implementation module.\n");
    return -1;
  };

  // register the publish method as event sink for msg_storage events
  AmArg es_args,ret;
  es_args.push(this);
  es_args.push("publish");
  MessageStorage->invoke("events_subscribe",es_args,ret);

  AmConfigReader cfg;

  use_domain = true;
  from_user = "mwi-publisher";
  to_user = "";
  route_set = "";
  presence_server = "";

  if(cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME ".conf"))) {
    INFO(MOD_NAME "configuration file (%s) not found, "
      "assuming default configuration is fine\n",
      (AmConfig::ModConfigPath + string(MOD_NAME ".conf")).c_str());

    return 0;
  };

  use_domain = cfg.getParameter("use_domain", "yes") == "yes";
  from_user  = cfg.getParameter("from_user", from_user);
  to_user  = cfg.getParameter("to_user", to_user);
  route_set  = cfg.getParameter("route_set", route_set);

  if (!use_domain) {
      presence_server = cfg.getParameter("presence_server", presence_server);
      if (presence_server.length() == 0) {
        ERROR("use domain set to false, but parameter 'presence_server' not found in the configuration file\n");
        return -1;
      }

      DBG("set presence server '%s'\n", presence_server.c_str());
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

    if (elem.get(2).asInt()) { // skip empty messages
      new_msgs += elem.get(1).asInt();
    }
    else {
      all_msgs--;
    }
  };

  DBG("Found %d new and %d old messages\n", new_msgs, all_msgs - new_msgs);
  string vm_buf = int2str(new_msgs) + "/" + int2str(all_msgs - new_msgs);

  headers = "Event: message-summary\r\n";
  headers += "Subscription-State: active\r\n";

  if (new_msgs > 0) {
    body = "Messages-Waiting: yes\r\n";
  }
  else {
    body = "Messages-Waiting: no\r\n";
  }

  body += "Message-Account: sip:" + user + "@" + domain + "\r\n";
  body += "Voice-Message: " + vm_buf + " (" + vm_buf + ")\r\n";

  AmMimeBody sms_body;
  sms_body.addPart("application/simple-message-summary");
  sms_body.setPayload((const unsigned char*) body.c_str(), body.length());

  string from_uri = "sip:";
  string to_uri   = "sip:";

  if (from_user.length() != 0) {
    from_uri += from_user + "@";
  }

  if (to_user.length() != 0) {
      to_uri += to_user + "@";
  }

  if (use_domain) {
      from_uri += domain;
      to_uri   += domain;
  }
  else {
      from_uri += presence_server;
      to_uri   += presence_server;
  }

  AmSipDialog tmp_d(NULL);
  tmp_d.setRemoteUri("sip:" + user + "@" + domain);
  tmp_d.setLocalParty("<" + from_uri + ">");
  tmp_d.setRemoteParty("<" + to_uri + ">");
  if (route_set.length() != 0) {
      tmp_d.setRouteSet(route_set);
  }
  tmp_d.setCallid(AmSession::getNewId() + "@" + AmConfig::SIP_Ifs[tmp_d.getOutboundIf()].getIP());
  tmp_d.setLocalTag(AmSession::getNewId());
  tmp_d.sendRequest(SIP_METH_PUBLISH, &sms_body, headers);
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
  else {
    throw AmDynInvoke::NotImplemented(method);
  }
};
