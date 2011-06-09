#ifndef _Rtmp_h_
#define _Rtmp_h_

#include "RtmpServer.h"
#include "AmApi.h"

class RtmpFactory
  : public AmSessionFactory
{

public:
  RtmpFactory();
  ~RtmpFactory();

  // from AmPluginFactory
  int onLoad();

  // from AmSessionFactory
  AmSession* onInvite(const AmSipRequest& req, const string& app_name,
		      const map<string,string>& app_params);
};

// declare the RtmpFactory as a singleton
typedef singleton<RtmpFactory> RtmpFactory_impl;

#endif
