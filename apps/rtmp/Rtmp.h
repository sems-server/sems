#ifndef _Rtmp_h_
#define _Rtmp_h_

#include "RtmpServer.h"
#include "AmApi.h"
#include "AmEventProcessingThread.h"

#define MOD_NAME "rtmp"
#define FACTORY_Q_NAME (MOD_NAME "_ev_proc")

class RtmpConnection;

struct RtmpConfig
{
  string FromName;
  string FromDomain;
  bool   AllowExternalRegister;
  string ListenAddress;
  unsigned int ListenPort;

  RtmpConfig();
};

class RtmpFactory
  : public AmSessionFactory,
    public AmEventProcessingThread
{
  // Global module configuration
  RtmpConfig cfg;

  // Container keeping trace of registered RTMP connections
  // to enable inbound calls to RTMP clients
  map<string,RtmpConnection*> connections;
  AmMutex                     m_connections;

protected:
  // @see AmEventProcessingThread
  void onEvent(AmEvent* ev);

public:
  RtmpFactory();
  ~RtmpFactory();

  // from AmPluginFactory
  int onLoad();

  // from AmSessionFactory
  AmSession* onInvite(const AmSipRequest& req, const string& app_name,
		      const map<string,string>& app_params);

  const RtmpConfig& getConfig() { return cfg; }
  
  int addConnection(const string& ident, RtmpConnection*);
  void removeConnection(const string& ident);
};

// declare the RtmpFactory as a singleton
typedef singleton<RtmpFactory> RtmpFactory_impl;

#endif
