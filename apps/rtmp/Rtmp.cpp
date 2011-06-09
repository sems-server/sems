#include "Rtmp.h"

#define MOD_NAME "rtmp"

extern "C" void* FACTORY_SESSION_EXPORT()
{
  return RtmpFactory_impl::instance();
}

RtmpFactory::RtmpFactory()
  : AmSessionFactory(MOD_NAME)
{
}

RtmpFactory::~RtmpFactory()
{
}

int RtmpFactory::onLoad()
{
  //TODO:
  // - Load some config
  // - pass at least the listen address(es) to the 
  //   RTMP server.
  RtmpServer::instance()->start();

  return 0;
}

AmSession* RtmpFactory::onInvite(const AmSipRequest& req, 
				 const string& app_name,
				 const map<string,string>& app_params)
{
  // TODO: lookup list of RTMP connections
  //       and find the callee.
  return NULL;
}
