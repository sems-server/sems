
#include <sys/poll.h>
#include <unistd.h>
#include <stdlib.h>

#include "AmUtils.h"
#include "AmConfig.h"
#include "AmSipHeaders.h"
#include "AmConfigReader.h"
#include "AmSession.h"
#include "sems.h"
#include "log.h"

#include "UnixCtrlInterface.h"

#ifndef MOD_NAME
#define MOD_NAME  "unixsockctrl"
#endif

EXPORT_CONTROL_INTERFACE_FACTORY(UnixCtrlInterfaceFactory, MOD_NAME);

/*
 * Config paramters names.
 */
#define SOCKET_NAME_PARAM       "socket_name"
#define REPLY_SOCKET_NAME_PARAM "reply_socket_name"
#define SER_SOCKET_NAME_PARAM   "ser_socket_name"
/*
 * Config paramters default values.
 */
#define SOCKET_NAME_DEFAULT       "/tmp/sems_sock"
#define REPLY_SOCKET_NAME_DEFAULT "/tmp/sems_rsp_sock"
#define SER_SOCKET_NAME_DEFAULT   "/tmp/ser_sock"

#define SIP_POLL_TIMEOUT    50 /*50 ms*/
#define MAX_MSG_ERR         5
#define SND_USOCK_TEMPLATE  "/tmp/sems_send_sock_XXXXXX" //TODO: configurable

//UnixCtrlInterface* UnixCtrlInterface::_instance = 0;

AmCtrlInterface* UnixCtrlInterfaceFactory::instance()
{
    UnixCtrlInterface* ctrl = new UnixCtrlInterface(reply_socket_name,ser_socket_name);

    if(ctrl->init(socket_name) < 0){

	delete ctrl;
	return NULL;
    }

    return ctrl;
}

int UnixCtrlInterfaceFactory::onLoad()
{
  AmConfigReader cfg;
  
  if (cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME ".conf"))) {
    WARN("failed to read/parse config file `%s' - assuming defaults\n",
        (AmConfig::ModConfigPath + string(MOD_NAME ".conf")).c_str());
    socket_name = string(SOCKET_NAME_DEFAULT);
    reply_socket_name = string(REPLY_SOCKET_NAME_DEFAULT);
    ser_socket_name = string(SER_SOCKET_NAME_DEFAULT);
  } else {
    socket_name = cfg.getParameter(SOCKET_NAME_PARAM, SOCKET_NAME_DEFAULT);
    reply_socket_name = cfg.getParameter(REPLY_SOCKET_NAME_PARAM,
      REPLY_SOCKET_NAME_DEFAULT);
    ser_socket_name = cfg.getParameter(SER_SOCKET_NAME_PARAM,
      SER_SOCKET_NAME_DEFAULT);
  }

  INFO(SOCKET_NAME_PARAM ": `%s'.\n", socket_name.c_str());
  INFO(REPLY_SOCKET_NAME_PARAM ": `%s'.\n", reply_socket_name.c_str());
  INFO(SER_SOCKET_NAME_PARAM ": `%s'.\n", ser_socket_name.c_str());

  return 0;
}


UnixCtrlInterface::~UnixCtrlInterface()
{
  reqAdapt.close();
  rplAdapt.close();
  sndAdapt.close();
}

UnixCtrlInterface::UnixCtrlInterface(const string& reply_socket_name, const string& ser_socket_name) 
    : reply_socket_name(reply_socket_name), ser_socket_name(ser_socket_name)
{
    sipDispatcher = AmSipDispatcher::instance();

}

int UnixCtrlInterface::init(const string& socket_name)
{
    //initialize the listening adapters
    if (! reqAdapt.init(socket_name)) {
	ERROR("failed to initialize requests listner.\n");
	return -1;
    }

    if (! rplAdapt.init(reply_socket_name)) {
	ERROR("failed to initialize replies listner.\n");
	return -1;
    }

    int errcnt = -1;
    char buff[sizeof(SND_USOCK_TEMPLATE)];
    do {
	if (MAX_MSG_ERR < ++errcnt) {
	    ERROR("failed to create a unix socket as a temproary file with "
		  "template `%s'.\n",SND_USOCK_TEMPLATE);

	    return -1;
	}

	//buff will hold the string for a temporary file that we'll than bind the
	//sending unix socket to. 
	//Bind is needed so that replies can be received from SER.
	memcpy(buff, SND_USOCK_TEMPLATE, sizeof(SND_USOCK_TEMPLATE));
	int fd = mkstemp(buff);
	if (0 <= fd) {
	    close(fd);
	    //by unlinking (here or in init(), there's a race created [the one
	    //mkstemp tries to fix :-) ) => try a bunch of times
	    unlink(buff);
	}
    } while (! sndAdapt.init(string(buff)));

    return 0;
}

int UnixCtrlInterface::send(const AmSipRequest &req, string &_)
{
  return rplAdapt.send(req, reply_socket_name, ser_socket_name);
}

int UnixCtrlInterface::send(const AmSipReply &rpl)
{
  return sndAdapt.send(rpl, ser_socket_name);
}

void UnixCtrlInterface::run()
{
  struct pollfd ufds[2];
  AmSipRequest req;
  AmSipReply rpl;

  if (! sipDispatcher) {
    ERROR("SIP dispacher hook not set.\n");
    return;
  }

#define POS_REQ 0
#define POS_RPL 1
  ufds[POS_REQ].fd = reqAdapt.getFd();
  ufds[POS_RPL].fd = rplAdapt.getFd();
  ufds[POS_REQ].events = POLLIN;
  ufds[POS_RPL].events = POLLIN;
  // FIXME: is this really needed?!
  ufds[POS_REQ].revents = 0;
  ufds[POS_RPL].revents = 0;

#define SERVICE_EVENT(_adapter, _msg) \
  do {  \
    if ((_adapter).receive(_msg)) \
      sipDispatcher->handleSipMsg(_msg); \
    else  \
      ERROR("failed to fetch %s.\n", #_msg);  \
  } while (0)

  DBG("Running UnixCtrlInterface thread.\n");
  while (!is_stopped()) {
    int ret = poll(ufds,2,/*FIXME: configurable*/SIP_POLL_TIMEOUT);
    switch (ret) {
      case -1:
        ERROR("AmServer: poll: %s\n",strerror(errno));
        continue;
      case 0:
        continue; //timedout
    case 1:
        if (ufds[POS_REQ].revents & POLLIN)
          SERVICE_EVENT(reqAdapt, req);
        else //rpl
          SERVICE_EVENT(rplAdapt, rpl);
        break;
    case 2:
        assert(ufds[POS_REQ].revents & POLLIN);
        assert(ufds[POS_RPL].revents & POLLIN);
        SERVICE_EVENT(reqAdapt, req);
        SERVICE_EVENT(rplAdapt, rpl);
        break;
    default:
        ERROR("unexpected poll events count: %i\n", ret);
        continue;
    }
  }

#undef SERVICE_EVENT
#undef POS_REQ
#undef POS_RPL
}

string UnixCtrlInterface::getContact(const string &displayName, 
    const string &userName, const string &hostName, 
    const string &uriParams, const string &hdrParams)
{
  string localUri;

  if (displayName.length()) {
    // quoting is safer (the check for quote need doesn't really pay off)
    if (displayName.c_str()[0] == '"') {
      assert(displayName.c_str()[displayName.length() - 1] == '"');
      localUri += displayName;
    } else {
      localUri += '"';
      localUri += displayName;
      localUri += '"';
    }
    localUri += " ";
  }

  // angular brackets not always needed (unless contact)
  localUri += "<";
  localUri += SIP_SCHEME_SIP; //TODO: sips|tel|tels
  localUri += ":";
  if (userName.length()) {
    localUri += userName;
    localUri += "@";
  }
  if (hostName.length())
    localUri += hostName;
  else
    localUri += "!!"; // Ser will replace that...

  if (uriParams.length()) {
    if (uriParams.c_str()[0] != ';')
      localUri += ';';
    localUri += uriParams;
  }
  localUri += ">";

  if (hdrParams.length()) {
    if (hdrParams.c_str()[0] != ';')
      localUri += ';';
    localUri += hdrParams;
  }

  return SIP_HDR_COLSP(SIP_HDR_CONTACT) + localUri + CRLF;
}
