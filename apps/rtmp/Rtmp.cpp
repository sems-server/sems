#include "Rtmp.h"
#include "RtmpSession.h"
#include "RtmpConnection.h"
#include "AmConfigReader.h"

RtmpConfig::RtmpConfig()
  : FromName("RTMP Gateway"),
    FromDomain(),
    AllowExternalRegister(false),
    ListenAddress("0.0.0.0"),
    ListenPort(DEFAULT_RTMP_PORT)
{
}

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
  AmConfigReader cfg_file;

  if(cfg_file.loadPluginConf(MOD_NAME) < 0){
    INFO("No config file for " MOD_NAME " plug-in: using defaults.\n");
  }
  else {

    if(cfg_file.hasParameter("from_name")){
      cfg.FromName = cfg_file.getParameter("from_name");
    }

    if(cfg_file.hasParameter("from_domain")){
      cfg.FromDomain = cfg_file.getParameter("from_domain");
    }

    if(cfg_file.hasParameter("allow_external_register")){
      cfg.AllowExternalRegister = 
	cfg_file.getParameter("allow_external_register") == string("yes");
    }

    if(cfg_file.hasParameter("listen_address")){
      cfg.ListenAddress = cfg_file.getParameter("listen_address");
    }

    if(cfg_file.hasParameter("listen_port")){
      string listen_port_str = cfg_file.getParameter("listen_port");
      if(sscanf(listen_port_str.c_str(),"%u",
		&(cfg.ListenPort)) != 1){
	ERROR("listen_port: invalid RTMP port specified (%s), using default\n",
	      listen_port_str.c_str());
	cfg.ListenPort = DEFAULT_RTMP_PORT;
      }
    }
  }

  RtmpServer* rtmp_server = RtmpServer::instance();
  
  if(rtmp_server->listen(cfg.ListenAddress.c_str(),cfg.ListenPort) < 0) {
    ERROR("could not start RTMP server at <%s:%u>\n",
	  cfg.ListenAddress.c_str(),cfg.ListenPort);
    rtmp_server->dispose();
    return -1;
  }
  rtmp_server->start();
  

  return 0;
}

AmSession* RtmpFactory::onInvite(const AmSipRequest& req, 
				 const string& app_name,
				 const map<string,string>& app_params)
{
  RtmpSession* sess=NULL;

  m_connections.lock();
  map<string,RtmpConnection*>::iterator it = connections.find(req.user);
  if(it != connections.end()){
    sess = new RtmpSession(it->second);
    it->second->setSessionPtr(sess);
    m_connections.unlock();
  }
  else {
    m_connections.unlock();
    AmSipDialog::reply_error(req,404,"Not found");
  }
  
  return sess;
}

int RtmpFactory::addConnection(const string& ident, RtmpConnection* conn)
{
  int res = 0;

  m_connections.lock();
  if(ident.empty() || (connections.find(ident)!=connections.end())){
    res = -1;
  }
  else {
    connections[ident] = conn;
  }
  m_connections.unlock();
  
  return res;
}

void RtmpFactory::removeConnection(const string& ident)
{
  m_connections.lock();
  connections.erase(ident);
  m_connections.unlock();
}
