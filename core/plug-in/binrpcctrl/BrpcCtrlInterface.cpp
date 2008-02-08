#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "AmUtils.h"
#include "AmConfig.h"
#include "AmConfigReader.h"
#include "AmSipHeaders.h"
#include "BrpcCtrlInterface.h"

#define MOD_NAME  "binrpcctrl"


EXPORT_CONTROL_INTERFACE_FACTORY(BrpcCtrlInterfaceFactory, MOD_NAME);


#define LISTEN_ADDR_PARAM   "sems_address"
#define SER_ADDR_PARAM      "ser_address"

#define LISTEN_ADDR_DEFAULT "brpcnd://127.0.0.1:3334"
#define SER_ADDR_DEFAULT    "brpcnd://127.0.0.1:1089"

#define BRPC_CB_HASH_SIZE   16
#define ASI_VERSION         0x2
#define SND_USOCK_TEMPLATE  "/tmp/sems_send_sock_XXXXXX" //TODO: configurable
#define MAX_RETRY_ON_ERR    5
//TODO: configurable
#define CT_TIMEOUT          500000
#define RX_TIMEOUT          500000 /* 50000 */
#define TX_TIMEOUT          200000

#ifndef UNIX_PATH_MAX
#include <sys/un.h>
#define UNIX_PATH_MAX sizeof(((struct sockaddr_un *)0)->sun_path)
#endif

#define STX   0x02
#define ETX   0x03
#define SUB   0x21

// ASI protocol methods
const BRPC_STR_STATIC_INIT(METH_SYNC, "asi.sync");
const BRPC_STR_STATIC_INIT(METH_METHODS, "methods");
const BRPC_STR_STATIC_INIT(METH_DIGESTS, "digests");
// SER RPC methods
const BRPC_STR_STATIC_INIT(METH_CORE_VER, "core.version");
const BRPC_STR_STATIC_INIT(METH_SER_RESYNC, "asi.resync");

//reply codes
enum RPC_ERR_CODE {
	CODE_RPC_SUCCESS = 200,
	CODE_RPC_INVALID = 400,
	CODE_RPC_FAILURE = 500,
};
//reply phrases
const BRPC_STR_STATIC_INIT(REASON_RPC_SUCCESS, "Success");
const BRPC_STR_STATIC_INIT(REASON_RPC_INVALID, "Invalid call");
const BRPC_STR_STATIC_INIT(REASON_RPC_FAILURE, "Internal Server Error");

const BRPC_STR_STATIC_INIT(SIP_REQUEST_REGISTER, "REGISTER");
const BRPC_STR_STATIC_INIT(SIP_REQUEST_INVITE, "INVITE");
const BRPC_STR_STATIC_INIT(SIP_REQUEST_CANCEL, "CANCEL");
const BRPC_STR_STATIC_INIT(SIP_REQUEST_ACK, "ACK");
const BRPC_STR_STATIC_INIT(SIP_REQUEST_INFO, "INFO");
const BRPC_STR_STATIC_INIT(SIP_REQUEST_BYE, "BYE");
const BRPC_STR_STATIC_INIT(SIP_REQUEST_PRACK, "PRACK");
const BRPC_STR_STATIC_INIT(SIP_REQUEST_REFER, "REFER");

//these are needed for requests
const BRPC_STR_STATIC_INIT(SER_DFMT_METHOD, "@method");
const BRPC_STR_STATIC_INIT(SER_DFMT_RURI_USER, "@ruri.user");
const BRPC_STR_STATIC_INIT(SER_DFMT_RURI_HOST, "@ruri.host");
const BRPC_STR_STATIC_INIT(SER_DFMT_RCV_IP, "@received.ip");
const BRPC_STR_STATIC_INIT(SER_DFMT_RCV_PORT, "@received.port");
const BRPC_STR_STATIC_INIT(SER_DFMT_RURI, "@ruri");
const BRPC_STR_STATIC_INIT(SER_DFMT_CONTACT_URI, "@hf_value.contact[1].uri");
const BRPC_STR_STATIC_INIT(SER_DFMT_FROM_URI, "@from.uri");
const BRPC_STR_STATIC_INIT(SER_DFMT_TO_URI, "@to.uri");
const BRPC_STR_STATIC_INIT(SER_DFMT_CALL_ID, "@call_id");
const BRPC_STR_STATIC_INIT(SER_DFMT_FROM_TAG, "@from.tag");
const BRPC_STR_STATIC_INIT(SER_DFMT_TO_TAG, "@to.tag");
const BRPC_STR_STATIC_INIT(SER_DFMT_CSEQ_NUM, "@cseq.num");
const BRPC_STR_STATIC_INIT(SER_DFMT_RR_ALL, "@hf_value.record_route");
const BRPC_STR_STATIC_INIT(SER_DFMT_RR_1ST, "@hf_value.record_route[1]");
const BRPC_STR_STATIC_INIT(SER_DFMT_BODY, "@msg.body");
const BRPC_STR_STATIC_INIT(SER_DFMT_CMD, "$sems_cmd");
const BRPC_STR_STATIC_INIT(SER_DFMT_HDRS, "$sems_hdrs");
//aditionals, for replies
const BRPC_STR_STATIC_INIT(SER_DFMT_CODE, "@code");
const BRPC_STR_STATIC_INIT(SER_DFMT_REASON, "@reason");


static const brpc_str_t *SIP_CORE_METHODS[] = {
  &SIP_REQUEST_REGISTER,
  &SIP_REQUEST_INVITE,
  &SIP_REQUEST_ACK, 
  &SIP_REQUEST_BYE,
  &SIP_REQUEST_CANCEL,
  &SIP_REQUEST_PRACK,
  &SIP_REQUEST_INFO,
  &SIP_REQUEST_REFER
};

static const brpc_str_t *REQ_FMTS[] = {
  &SER_DFMT_METHOD,
  &SER_DFMT_RURI_USER,
  &SER_DFMT_RURI_HOST,
  &SER_DFMT_RCV_IP,
  &SER_DFMT_RCV_PORT,
  &SER_DFMT_RURI,
  &SER_DFMT_CONTACT_URI,
  &SER_DFMT_FROM_URI,
  &SER_DFMT_TO_URI,
  &SER_DFMT_CALL_ID,
  &SER_DFMT_FROM_TAG,
  &SER_DFMT_TO_TAG,
  &SER_DFMT_CSEQ_NUM,
  &SER_DFMT_RR_ALL,
  &SER_DFMT_RR_1ST,
  &SER_DFMT_BODY,
  &SER_DFMT_CMD,
  &SER_DFMT_HDRS
};

static const brpc_str_t *FIN_FMTS[] = {
  &SER_DFMT_CODE,
  &SER_DFMT_REASON,
  &SER_DFMT_CONTACT_URI,
  &SER_DFMT_RR_1ST,
  &SER_DFMT_RR_ALL,
  &SER_DFMT_FROM_TAG,
  &SER_DFMT_TO_TAG,
  &SER_DFMT_CSEQ_NUM,
  &SER_DFMT_HDRS,
  &SER_DFMT_BODY
};

#define PROV_FMTS FIN_FMTS


enum SIP_METHOD_TYPE {
  SIP_METH_NONE, /* AS not interested */

  SIP_METH_REQ,
  SIP_METH_FIN,
  SIP_METH_PRV,

  SIP_METH_MAX /* indicates "out of bounds" */
};

/* WARN: must remain sync'ed with SER's enum ASI_REQ_FLAGS! */
enum SIP_REQ_FLAGS {
  SIP_REQ_ACK_FLG = 1 << 0,
  SIP_REQ_FIN_FLG = 1 << 1,
  SIP_REQ_PRV_FLG = 1 << 2,
  SIP_REQ_RUN_ORR = 1 << 3,
};


const BRPC_STR_STATIC_INIT(SER_REQUEST, "asi.uac.request");
const BRPC_STR_STATIC_INIT(SER_CANCEL, "asi.uac.cancel");
const BRPC_STR_STATIC_INIT(SER_ACK, "asi.uac.ack");
const BRPC_STR_STATIC_INIT(SER_REPLY, "asi.uas.reply");

/**
 * All replies for SIP requests (including CANCEL and ACK) are of format: 
 * 	<code> (integer)
 * 	<reason> (string).
 * The failure codes are larger than 299 (SIP error codes).
 * The reason is something usefull only for successfull requests (as it 
 * contains SER's TM ID); for the rest of the cases, it's just an explanation
 * message.
 */
const static char FMT_RPL[] = "ds";
const static char REQUEST_FMT_REQ[] = "ddssssdsssss";
const static char REPLY_FMT_REQ[] = "sdssss";
const static char CANCEL_FMT_REQ[] = "s";

#define STR2BSTR(bstr, _str)  \
  brpc_str_t bstr = {const_cast<char *>((_str).c_str()), (_str).length()}; \
  DBG("%s: `%.*s'.\n", #_str, BRPC_STR_FMT(&bstr))

#define CONFIRM_RECEPTION 0

// time_t BrpcCtrlInterface::serial = -1;
// brpc_int_t BrpcCtrlInterface::as_id = -1;

BrpcCtrlInterfaceFactory::BrpcCtrlInterfaceFactory(const string &name) 
    : AmCtrlInterfaceFactory(name)
{
}

BrpcCtrlInterfaceFactory::~BrpcCtrlInterfaceFactory()
{
}

AmCtrlInterface* BrpcCtrlInterfaceFactory::instance()
{
    BrpcCtrlInterface* ctrl = new BrpcCtrlInterface();
    
    if(ctrl->init(semsUri,serUri) < 0){
	delete ctrl;
	return NULL;
    }

    return ctrl;
}


BrpcCtrlInterface::BrpcCtrlInterface()
    : semsFd(-1),
      serFd(-1),
      serial(-1),
      as_id(-1)
{
    memset(&sndAddr, 0, sizeof(sndAddr));
}
BrpcCtrlInterface::~BrpcCtrlInterface()
{
    closeSock(&serFd, &sndAddr);
    closeSock(&semsFd, &semsAddr);
}


static int init_listener(const string &semsUri, brpc_addr_t *semsAddr)
{
  brpc_addr_t *addr;
  int sockfd;

  if (! (addr = brpc_parse_uri(semsUri.c_str()))) {
    ERROR("failed to parse BINRPC URI `%s': %s [%d].\n", semsUri.c_str(),
      brpc_strerror(), brpc_errno);
    return -1;
  } else if (BRPC_ADDR_TYPE(addr) != SOCK_DGRAM) {
    //b/c we'd have to do connection management otherwise (listen for
    //connections, poll for activity on each descriptor etc); for now, not
    //relly needed.
    ERROR("only datagram listeners supported.\n");
    return -1;
  }

  if ((sockfd = brpc_socket(addr, /*blocking*/false, /*bind*/true)) < 0) {
    ERROR("failed to get listen socket for URI `%s': %s [%d].\n", 
      semsUri.c_str(), brpc_strerror(), brpc_errno);
    return -1;
  }
  *semsAddr = *addr;
  return sockfd;
}

int BrpcCtrlInterface::init(const string& semsUri, const string& serUri)
{
  brpc_addr_t *addr;

  if ((semsFd = init_listener(semsUri, &semsAddr)) < 0) {
    ERROR("failed to initialize BINRPC listening socket.\n");
    return -1;
  }

  if (! (addr = brpc_parse_uri(serUri.c_str()))) {
    ERROR("failed to parse BINRPC URI `%s': %s [%d].\n", serUri.c_str(),
      brpc_strerror(), brpc_errno);
    return -1;
  } else {
    serAddr = *addr;
  }

  sipDispatcher = AmSipDispatcher::instance();

  return 0;
}

int BrpcCtrlInterfaceFactory::onLoad()
{
  AmConfigReader cfg;

  if (cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME ".conf"))) {
    WARN("failed to read/parse config file `%s' - assuming defaults\n",
      (AmConfig::ModConfigPath + string(MOD_NAME ".conf")).c_str());
    semsUri = string(LISTEN_ADDR_DEFAULT);
    serUri = string(SER_ADDR_DEFAULT);
  } else {
    semsUri = cfg.getParameter(LISTEN_ADDR_PARAM, LISTEN_ADDR_DEFAULT);
    serUri = cfg.getParameter(SER_ADDR_PARAM, SER_ADDR_DEFAULT);
  }
  INFO(LISTEN_ADDR_PARAM ": %s.\n", semsUri.c_str());
  INFO(SER_ADDR_PARAM ": %s.\n", serUri.c_str());

  return 0;
}


int BrpcCtrlInterface::getSerFd()
{
  if (0 <= serFd)
    return serFd;

  //if using local sockets, we need to bind to a local socket when sending
  //out requests, so that SER knows where to send back replies for our
  //requests (unlike INET layers PF_LOCAL+DGRAM based communication needs to
  //bind.the sending socket in order to have this socket receive replies)
  if (BRPC_ADDR_DOMAIN(&serAddr) == PF_LOCAL) {
    //There's a race the loop tries to work around:
    //1. mkstemp creates & opens a temp file. brpc_socket() (called
    //later), removes the temporary file (unlink) and opens a unix 
    //socket with the same name.
    //So, between unlink() and socket(), some other mkstemp could
    //"ocupy" the name.
    int errcnt = -1;
    do {
      if (MAX_RETRY_ON_ERR < ++errcnt) {
        ERROR("%dth consecutive failed attempt to create a local domain "
          "socket for sending req - giving up.\n", errcnt);
        return -1;
      } else if (1 < errcnt) { //the previous brpc_socket() attempt failed
        ERROR("failed to create BINRPC socket: %s [%d].\n", brpc_strerror(),
          brpc_errno);
      }
      char buff[UNIX_PATH_MAX];
      assert(sizeof(SND_USOCK_TEMPLATE) <= sizeof(buff));
      memcpy(buff, SND_USOCK_TEMPLATE, sizeof(SND_USOCK_TEMPLATE));
      int tmpfd = mkstemp(buff);
      if (tmpfd < 0) {
        ERROR("failed to create temporary file with template `%s': %s [%d].\n",
            SND_USOCK_TEMPLATE, strerror(errno), errno);
        continue;
      } else {
        //close the FD - only the modified buff is of worth
        close(tmpfd);
      }
      sndAddr = serAddr; //copy domain, socket type
      memcpy(BRPC_ADDR_UN(&sndAddr)->sun_path, buff, strlen(buff) + 
        /*0-term*/1);
      BRPC_ADDR_LEN(&sndAddr) = SUN_LEN(BRPC_ADDR_UN(&sndAddr));
      DBG("creating temporary send socket bound to `%s'.\n", buff);
    } while ((serFd = brpc_socket(&sndAddr, /*blocking*/false, 
        /*named*/true)) < 0);
    /* TODO: permission, UID/GID of the created socket */
  } else {
    if ((serFd = brpc_socket(&serAddr, /*blk*/false, /*named*/false)) < 0) {
      ERROR("failed to create BINRPC socket: %s [%d].\n", brpc_strerror(),
        brpc_errno);
      return -1;
    }
  }
  if (! brpc_connect(&serAddr, &serFd, CT_TIMEOUT)) {
    ERROR("failed to connect to SER: %s [%d].\n", brpc_strerror(), brpc_errno);
    close(serFd);
    serFd = -1;
  }
  return serFd;
}

void BrpcCtrlInterface::closeSock(int *sock, brpc_addr_t *addr)
{
  DBG("closing FD#%d for %s.\n", *sock, 
    BRPC_ADDR_DOMAIN(addr) ? brpc_print_addr(addr) : "[INET/INET6 sender]");

  if (*sock < 0) {
    WARN("connection not oppened, so can not be closed.\n");
    return;
  }

  if (close(*sock) < 0)
    WARN("FD closed uncleanly: %s [%d].\n", strerror(errno), errno);
  
  if (BRPC_ADDR_DOMAIN(addr) == PF_LOCAL) {
    if (unlink(BRPC_ADDR_UN(addr)->sun_path) < 0) {
      ERROR("failed to remove unix socket file '%s': %s [%d].\n", 
        BRPC_ADDR_UN(addr)->sun_path, strerror(errno), errno);
    }
  }
  
  DBG("socket %d closed.\n", *sock);
  *sock = -1;
}

bool BrpcCtrlInterface::initCallbacks()
{
  if (! brpc_cb_init(BRPC_CB_HASH_SIZE, /*no reply handling*/0)) {
    ERROR("failed to initialize BINRPC callbacks: %s [%d].\n", brpc_strerror(),
      brpc_errno);
    return false;
  }

  serial = time(NULL);

  if (! (brpc_cb_req(METH_SYNC.val, NULL, asiSync, NULL, this) &&
      brpc_cb_req(METH_METHODS.val, NULL, (brpc_cb_req_f)&methods, NULL, 
          this) &&
      brpc_cb_req(METH_DIGESTS.val, NULL, (brpc_cb_req_f)&digests, NULL, 
          this) &&
      brpc_cb_req(SIP_REQUEST_REGISTER.val, NULL, 
          (brpc_cb_req_f)&req_handler, NULL, this) &&
      brpc_cb_req(SIP_REQUEST_INVITE.val, NULL, 
          (brpc_cb_req_f)&req_handler, NULL, this) &&
      brpc_cb_req(SIP_REQUEST_CANCEL.val, NULL, 
          (brpc_cb_req_f)&req_handler, NULL, this) &&
      brpc_cb_req(SIP_REQUEST_ACK.val, NULL, 
          (brpc_cb_req_f)&req_handler, NULL, this) &&
      brpc_cb_req(SIP_REQUEST_INFO.val, NULL, 
          (brpc_cb_req_f)&req_handler, NULL, this) &&
      brpc_cb_req(SIP_REQUEST_BYE.val, NULL, 
          (brpc_cb_req_f)&req_handler, NULL, this) &&
      brpc_cb_req(SIP_REQUEST_PRACK.val, NULL, 
          (brpc_cb_req_f)&req_handler, NULL, this) &&
      brpc_cb_req(SIP_REQUEST_REFER.val, NULL, 
          (brpc_cb_req_f)&req_handler, NULL, this) &&
      1)) {
    ERROR("failed to register the BINRPC callbaks: %s [%d].\n", 
      brpc_strerror(), brpc_errno);
    return false;
  }
 
  return true;
}


static inline brpc_t *build_reply(brpc_t *req, enum RPC_ERR_CODE errcode)
{
  brpc_t *rpl = NULL;
  const brpc_str_t *reason;
  brpc_int_t errc;

  switch (errcode) {
    case CODE_RPC_SUCCESS:
      if (! ((rpl = brpc_rpl(req)) && brpc_asm(rpl, "ds", 
          CODE_RPC_SUCCESS, &REASON_RPC_SUCCESS)))
        goto err;
      break;
    do {
    case CODE_RPC_INVALID: reason = &REASON_RPC_INVALID; break;
    case CODE_RPC_FAILURE: reason = &REASON_RPC_FAILURE; break;
    } while (0);
      errc = errcode;
      if (! ((rpl = brpc_rpl(req)) && brpc_fault(rpl, &errc, reason)))
        goto err;
      break;
    default:
      ERROR("CRITICAL: unexpected error code: %d.\n", errcode);
#ifndef NDEBUG
      abort();
#endif
  }

  return rpl;
err:
  ERROR("failed to build reply: %s [%d].\n", brpc_strerror(), brpc_errno);
  if (rpl)
    brpc_finish(rpl);
  return NULL;
}


#define GOTOERR(_code) \
  do { \
    errcode = _code; \
    goto err; \
  } while (0)
#define ERRHANDLER(_msg_) \
  do { \
    ERROR(_msg_ ": %d (%s [%d]).\n", errcode, brpc_strerror(), brpc_errno); \
    if (rpl) \
      brpc_finish(rpl); \
    return build_reply(req, errcode); \
  } while (0)


brpc_t *BrpcCtrlInterface::asiSync(brpc_t *req, void *_iface)
{
  brpc_int_t *id, *proto;
  time_t ret_proto, ret_serial;
  enum RPC_ERR_CODE errcode;
  brpc_t *rpl = NULL;
  BrpcCtrlInterface *iface = (BrpcCtrlInterface *)_iface;

  if (! brpc_dsm(req, "dd", &proto, &id))
    GOTOERR((brpc_errno == EINVAL) ? CODE_RPC_INVALID : CODE_RPC_FAILURE);
  if (! proto)
    GOTOERR(CODE_RPC_INVALID);
  if (! id)
    GOTOERR(CODE_RPC_INVALID);

  DBG("SER supported protocol: %lu.\n", (long unsigned)*proto);
  if (*proto != ASI_VERSION) {
    ret_proto = 0;
    ret_serial = -1;
  } else {
    iface->as_id = *id;
    ret_proto = ASI_VERSION;
    ret_serial = iface->serial;
    DBG("SER assigned SEMS the AS ID: %lu.\n", (long unsigned)*id);
  }
  DBG("SEMS returning serial: %lu.\n", (long unsigned)ret_serial);
  
  if (! (rpl = brpc_rpl(req)))
    GOTOERR(CODE_RPC_FAILURE);

  if (! brpc_asm(rpl, "dd", ret_proto, ret_serial))
    GOTOERR(CODE_RPC_FAILURE);

  return rpl;
err:
  ERRHANDLER("failed to sync");
}

brpc_t *BrpcCtrlInterface::methods(brpc_t *req, void *iface)
{
  brpc_t *rpl = NULL;
  enum RPC_ERR_CODE errcode;
  unsigned int i;

  if (! (rpl = brpc_rpl(req)))
    GOTOERR(CODE_RPC_FAILURE);

  for (i = 0; i < sizeof(SIP_CORE_METHODS)/sizeof(brpc_str_t *); i ++)
    if (! brpc_asm(rpl, "s", SIP_CORE_METHODS[i]))
      GOTOERR(CODE_RPC_FAILURE);
  return rpl;
err:
  ERRHANDLER("failed to return supported SIP methods");
}


static brpc_val_t *digests_avp(enum SIP_METHOD_TYPE type, size_t cnt, 
    const brpc_str_t **digs)
{
  brpc_val_t *list, *id = NULL, *avp = NULL, *desc = NULL;
  char id_str[2] = "?";
  unsigned int i;

  id_str[0] = '0' + type;
  if (! ((list = brpc_list(NULL)) && 
      (id = brpc_str(id_str, sizeof(id_str))))) {
    goto err;
  } else if (! (avp = brpc_avp(id, list))) {
    goto err;
  } else {
    id = NULL;
  }

  for (i = 0; i < cnt; i ++) {
    if (! (  (desc = brpc_str(digs[i]->val, digs[i]->len)) && 
        brpc_list_add(list, desc))) {
      goto err;
    } else {
      desc = NULL;
    }
  }
  return avp;
err:
  if (avp) {
    if (desc)
      brpc_val_free(desc);
    brpc_val_free(avp);
  } else {
    if (list)
      brpc_val_free(list);
    if (id)
      brpc_val_free(id);
  }
  return NULL;
}

brpc_t *BrpcCtrlInterface::digests(brpc_t *req, void *iface)
{
  brpc_str_t *mname;//, *meth;
  brpc_t *rpl = NULL;
  brpc_val_t *avp = NULL, *map = NULL, *mapptr;
  enum RPC_ERR_CODE errcode;
  unsigned int i;

  if (! brpc_dsm(req, "s", &mname))
    GOTOERR((brpc_errno == EINVAL) ? CODE_RPC_INVALID : CODE_RPC_FAILURE);
  if (! mname)
    GOTOERR(CODE_RPC_INVALID);

  // check if not a bogus call
  for (i = 0; i <  sizeof(SIP_CORE_METHODS)/sizeof(brpc_str_t *); i ++) {
    if ((mname->len == SIP_CORE_METHODS[i]->len) &&
        (strncmp(mname->val, SIP_CORE_METHODS[i]->val, mname->len) == 0)) {
      break;
    }
  }
  if (i == sizeof(SIP_CORE_METHODS)/sizeof(brpc_str_t *))
    GOTOERR(CODE_RPC_INVALID);

  if (! (  (rpl = brpc_rpl(req)) && (map = brpc_map(NULL)) && 
      brpc_add_val(rpl, map))) {
    GOTOERR(CODE_RPC_FAILURE);
  } else {
    mapptr = map;
    map = NULL;
  }

  /* requests */
  if (! (avp = digests_avp(SIP_METH_REQ, sizeof(REQ_FMTS)/sizeof(brpc_str_t *),
      REQ_FMTS)))
    GOTOERR(CODE_RPC_FAILURE);
  else if (! brpc_map_add(mapptr, avp))
    GOTOERR(CODE_RPC_FAILURE);
  else
    avp = NULL;
  /* finals */
  if (! (avp = digests_avp(SIP_METH_FIN, sizeof(FIN_FMTS)/sizeof(brpc_str_t *),
      FIN_FMTS)))
    GOTOERR(CODE_RPC_FAILURE);
  else if (! brpc_map_add(mapptr, avp))
    GOTOERR(CODE_RPC_FAILURE);
  else
    avp = NULL;
  /* provisionals */
  if (! (avp = digests_avp(SIP_METH_PRV, 
      sizeof(PROV_FMTS)/sizeof(brpc_str_t *), PROV_FMTS)))
    GOTOERR(CODE_RPC_FAILURE);
  else if (! brpc_map_add(mapptr, avp))
    GOTOERR(CODE_RPC_FAILURE);
  else
    avp = NULL;

  return rpl;
err:
  if (rpl) {
    if (map)
      brpc_val_free(map);
    if (avp)
      brpc_val_free(avp);
    /* rpl free'ed by ERRHANDLER */
  }
  ERRHANDLER("failed to return supported SIP methods");
}


/**
 * Executes a BINRPC request.
 * @param req BINRPC request to be executed; the function takes care of it's
 * disposal
 * @return Result returned by server; NULL in case of error.
 */
brpc_t *BrpcCtrlInterface::rpcExecute(brpc_t *req)
{
  brpc_t *rpl = NULL;
  brpc_str_t *reason;
  brpc_int_t *code;
  brpc_addr_t from = serAddr; //avoid a syscall to find out socket type
  brpc_id_t req_id;

  if (getSerFd() < 0) {
    ERROR("no connection to SER available.\n");
    goto end;
  }
  assert(0 <= serFd);

  if (! brpc_sendto(serFd, &serAddr, req, TX_TIMEOUT)) {
    ERROR("failed to send msg to SER: %s [%d].\n", brpc_strerror(), 
      brpc_errno);
    closeSock(&serFd, &sndAddr);
    goto end;
  } else {
    req_id = req->id;
    brpc_finish(req);
    req = NULL;
  }
  
  /* receive from queue until empty, if IDs do not match */
  while ((rpl = brpc_recvfrom(serFd, &from, RX_TIMEOUT))) {
    if (req_id == rpl->id)
      break;
    ERROR("received reply's ID (#%d) doesn't match request's - discarded (%d)",
        brpc_id(rpl), req_id);
    brpc_finish(rpl);
  }
  if (! rpl) {
    ERROR("failed to get reply: %s [%d].\n", brpc_strerror(), brpc_errno);
    closeSock(&serFd, &sndAddr);
    goto end;
  }
  if (brpc_is_fault(rpl)) {
    ERROR("RPC call ID#%d faulted.\n", brpc_id(rpl));
    if (brpc_fault_status(rpl, &code, &reason)) {
      if (code)
        ERROR("RPC ID#%d failure code: %d.\n", brpc_id(rpl), *code);
      if (reason)
        ERROR("RPC ID#%d failure reason: %.*s.\n", brpc_id(rpl), 
          BRPC_STR_FMT(reason));
    }
    brpc_finish(rpl);
    rpl = NULL;
    goto end;
  }
  DBG("RPC successfully executed.\n");
end:
  if (req)
    brpc_finish(req);
  return rpl;
}


bool BrpcCtrlInterface::rpcCheck()
{
  brpc_t *req, *rpl;
  char *version;
  bool ret = false;

  if (! (req = brpc_req(METH_CORE_VER, random()))) {
    ERROR("failed to build '%.*s' RPC context: %s [%d].\n", 
      (int)METH_CORE_VER.len, METH_CORE_VER.val, brpc_strerror(), brpc_errno);
    return false;
  }
  if (! (rpl = rpcExecute(req)))
    return false;
  if (! brpc_dsm(rpl, "c", &version)) {
    ERROR("failed to retrieve version: %s [%d].\n", brpc_strerror(), 
      brpc_errno);
    goto end;
  }
  if (! version) {
    ERROR("unexpected NULL string as SER version.\n");
    goto end;
  }
  INFO("SER Version: %s\n", version);
  ret = true;
end:
  if (rpl)
    brpc_finish(rpl);
  return ret;
}

void BrpcCtrlInterface::serResync()
{
  brpc_t *req, *rpl = NULL;
  brpc_str_t listen, *reason;
  int *retcode;

  listen.val = brpc_print_addr(&semsAddr);
  listen.len = strlen(listen.val);

  if (! ((req = brpc_req(METH_SER_RESYNC, random())) && 
      brpc_asm(req, "dsd", ASI_VERSION, &listen, serial))) {
    ERROR("failed to build '%.*s' RPC context: %s [%d].\n", 
	  (int)METH_SER_RESYNC.len, METH_SER_RESYNC.val, brpc_strerror(), brpc_errno);
    goto err;
  }

  if (! (rpl = rpcExecute(req)))
    goto err;

  if (! brpc_dsm(rpl, "ds", &retcode, &reason)) {
    ERROR("failed disassemble reply: %s [%d].\n", brpc_strerror(), brpc_errno);
    goto err;
  }
  if (! retcode) {
    ERROR("invalid return code (NULL).\n");
    goto err;
  }
  if (*retcode / 100 == 2) { // was success
    char *endptr;
    long my_as_id;
    errno = 0;
    my_as_id = strtol(reason->val, &endptr, 10);
    if (*endptr || errno) {
      ERROR("failed to parse AS ID returned by SER (%d: %s).\n", errno, 
          errno ? strerror(errno) : "unexpected characters");
      goto err;
    } else {
      as_id = (int)my_as_id;
    }
  } else {
    ERROR("resync failed with %d: %s.\n", *retcode, 
        reason ? reason->val : "[NULL]");
    goto err;
  }

  INFO("SER resync reply: %d: %.*s\n", *retcode, BRPC_STR_FMT(reason));
  brpc_finish(rpl);
  return;
err:
  ERROR("failed to execute SER resync.\n");
  if (rpl)
    brpc_finish(rpl);
}

void BrpcCtrlInterface::_run()
{
  brpc_addr_t from;
  brpc_t *req, *rpl;

  DBG("Running BrpcCtrlInterface thread.\n");
  while (! is_stopped()) {
    from = semsAddr; // avoid a syscall to find out socket type
    if (! (req = brpc_recvfrom(semsFd, &from, RX_TIMEOUT)))
      continue;
    //unsafe
    DBG("received BINRPC request `%.*s'.\n", BRPC_STR_FMT(brpc_method(req)));
    if ((rpl = brpc_cb_run(req))) {
      if (! brpc_sendto(semsFd, &from, rpl, TX_TIMEOUT)) {
        ERROR("failed to send reply to BINRPC request: %s [%d].\n",
          brpc_strerror(), brpc_errno);
      }
      brpc_finish(rpl);
    }
    brpc_finish(req);
  }
}

void BrpcCtrlInterface::run()
{
  if (! sipDispatcher) {
    ERROR("SIP dispatcher hook not set.\n");
    return;
  }

  if (! initCallbacks()) {
    ERROR("failed to init BINRPC call back system\n");
    return;
  }

  if(rpcCheck())
    serResync();

  _run();
}

static inline enum RPC_ERR_CODE read_unsigned(string &u_str, 
    unsigned int &u_int)
{
  const char *u_cstr = u_str.c_str();
  char *endptr;
  long long u_ll = strtoll(u_cstr, &endptr, /*nr. base*/10);
  if (endptr != &u_cstr[u_str.length()/*len() doesn't count 0-term*/]) {
    ERROR("invalid digest `%s' as unsigned - not a number.\n", u_cstr);
    return CODE_RPC_INVALID;
  } else if (u_ll < 0) {
    ERROR("invalid unsigned representation `%s' - negative [%lld].\n", 
      u_cstr, u_ll);
    return CODE_RPC_INVALID;
  } else {
    u_int = (typeof(u_int))u_ll;
  }
  return CODE_RPC_SUCCESS;
}

/**
 * All SER invoked RPCs have three leading values (besides requested by the
 * digest specifiers), as follows:
 * #1: discriminator (request, final, provisional reply);
 * #2: SER's opaque; must be read from SIP requests and returned in SIP
 * replies;
 * #3: AS's opaque; own opaque only makes sense for SIP replies;
 */
static enum RPC_ERR_CODE sip_req_handler(brpc_t *brpc_req, AmSipRequest &amReq)
{
  const static size_t FMT_LEAD_LEN = 
    /*`!'*/1 + 
    /* `.' x2 */2 + 
    /*SER's TID*/1;
  char fmt[FMT_LEAD_LEN + sizeof(REQ_FMTS)/sizeof(brpc_str_t *) + /*0-term*/1];
  string cseq_str;
  string *strRef[] = {
    &amReq.serKey,
    &amReq.method,
    &amReq.user,
    &amReq.domain,
    &amReq.dstip,
    &amReq.port,
    &amReq.r_uri,
    &amReq.from_uri,
    &amReq.from,
    &amReq.to,
    &amReq.callid,
    &amReq.from_tag,
    &amReq.to_tag,
    &cseq_str,
    &amReq.route,
    &amReq.next_hop,
    &amReq.body,
    &amReq.cmd,
    &amReq.hdrs
  };
  brpc_str_t *cstr_refs[sizeof(strRef)/sizeof(string *)];

#ifndef NDEBUG
  assert(sizeof(strRef)/sizeof(string *) - /*implicit TID*/1 == 
    sizeof(REQ_FMTS)/sizeof(brpc_str_t *));
#endif

  memset(fmt, 's', sizeof(fmt)/sizeof(char) - 1);
  fmt[0] = '!'; /* lay the refs in array */
  fmt[1] = '.'; /* ignore discriminator (had been already read by now) */
  /* 2nd pos: SER's opaque (TID); makes sense only for non-ACK methods*/
  fmt[3] = '.'; /* ignore AS opaque for requests */
  fmt[sizeof(fmt)/sizeof(char) - 1] = 0;

  if (! brpc_dsm(brpc_req, fmt, cstr_refs)) {
    ERROR("failed to disassemble RPC message: %s [%d].\n", brpc_strerror(),
        brpc_errno);
    return CODE_RPC_INVALID;
  }

  for (unsigned i = 0; i < sizeof(strRef)/sizeof(string *); i ++) {
    if (cstr_refs[i])
      strRef[i]->assign(cstr_refs[i]->val, cstr_refs[i]->len - /*no 0-term*/1);
    DBG("#%u: `%s'\n", i, strRef[i]->c_str());
  }

  enum RPC_ERR_CODE errcode;
  if ((errcode = read_unsigned(cseq_str, amReq.cseq)) != CODE_RPC_SUCCESS) {
    ERROR("failed to read CSeq value.\n");
    return errcode;
  }
  return CODE_RPC_SUCCESS;
}

static enum RPC_ERR_CODE sip_fin_handler(brpc_t *brpc_req, AmSipReply &amRpl)
{
  const static size_t FMT_LEAD_LEN =
    /*`!'*/1 + 
    /*`.'*/1 + 
    /*AS'es + SER's opaque*/2;
  char fmt[FMT_LEAD_LEN + sizeof(FIN_FMTS)/sizeof(brpc_str_t *) + /*0-term*/1];
  string opaque, code_str, cseq_str;
  string *strRef[] = {
    &amRpl.serKey,
    &opaque,
    &code_str,
    &amRpl.reason,
    &amRpl.next_request_uri,
    &amRpl.next_hop,
    &amRpl.route,
    &amRpl.local_tag,
    &amRpl.remote_tag,
    &cseq_str,
    &amRpl.hdrs,
    &amRpl.body
  };
  brpc_str_t *cstr_refs[sizeof(strRef)/sizeof(string *)];

  memset(fmt, 's', sizeof(fmt)/sizeof(char));
  fmt[0] = '!';
  fmt[1] = '.'; // discriminator had already been fetched by now
  fmt[sizeof(fmt)/sizeof(char) - 1] = 0;

  if (! brpc_dsm(brpc_req, fmt, cstr_refs)) {
    ERROR("failed to disassemble RPC message: %s [%d].\n", brpc_strerror(),
        brpc_errno);
    return CODE_RPC_INVALID;
  }

  for (unsigned i = 0; i < sizeof(strRef)/sizeof(string *); i ++) {
    if (cstr_refs[i])
      strRef[i]->assign(cstr_refs[i]->val, cstr_refs[i]->len - /*no 0-term*/1);
    DBG("#%u: `%.*s'\n", i, (int)strRef[i]->length(), strRef[i]->c_str());
  }

  enum RPC_ERR_CODE errcode;
  if ((errcode = read_unsigned(cseq_str, amRpl.cseq)) != CODE_RPC_SUCCESS) {
    ERROR("failed to read CSeq digest value.\n");
    return errcode;
  }
  if ((errcode = read_unsigned(code_str, amRpl.code)) != CODE_RPC_SUCCESS) {
    ERROR("failed to read code digest value.\n");
    return errcode;
  }

  return CODE_RPC_SUCCESS;
}

/**
 * @return SIP method type or:
 *  SIP_METH_MAX : for internal failure.
 *  SIP_METH_NONE : for invalid call
 */
static inline int get_sipmeth_type(brpc_t *req)
{
  const brpc_val_t *val;

  /* extract message type [REQ|FIN|RPL] */
  if (brpc_val_cnt(req) < /*discriminator*/1)
    return SIP_METH_NONE;
  if (! (val = brpc_fetch_val(req, /* discriminator index */0)))
    return SIP_METH_MAX;
  if (brpc_val_type(val) != BRPC_VAL_INT) {
    ERROR("unexpected SIP method type discriminator (type: %d; "
        "expected: %d).\n", brpc_val_type(val), BRPC_VAL_INT);
    return SIP_METH_NONE;
  }
  if (brpc_is_null(val)) {
    ERROR( "unexpected NULL value as SIP method type discriminator.\n");
    return SIP_METH_NONE;
  }
  return brpc_int_val(val);
}

#define BUILD_REPLY(_req, _code)  \
  ((CONFIRM_RECEPTION) ? build_reply(_req, _code) : NULL)


brpc_t *BrpcCtrlInterface::req_handler(brpc_t *req, void *_iface)
{
  enum RPC_ERR_CODE errcode;
  int mtype;
  AmSipRequest amReq;
  AmSipReply amRpl;
  BrpcCtrlInterface *iface = (BrpcCtrlInterface *)_iface;

  switch ((mtype = get_sipmeth_type(req))) {
    case SIP_METH_REQ:
      if ((errcode = sip_req_handler(req, amReq)) == CODE_RPC_SUCCESS)
        iface->handleSipMsg(amReq);
      break;

    case SIP_METH_FIN:
    case SIP_METH_PRV:
      if ((errcode = sip_fin_handler(req, amRpl)) == CODE_RPC_SUCCESS)
        iface->handleSipMsg(amRpl);
      break;

    case SIP_METH_NONE: 
      errcode = CODE_RPC_INVALID; 
      break;
    default: 
      errcode = CODE_RPC_FAILURE;
  }

  return BUILD_REPLY(req, errcode);
}


static inline brpc_t *build_cancel(const AmSipRequest &amReq)
{
  brpc_t *req;

  if (! (req = brpc_req(SER_CANCEL, random()))) {
    ERROR("failed to build RPC context: %s [%d].\n", brpc_strerror(), 
        brpc_errno);
    return NULL;
  }

  STR2BSTR(_serKey, amReq.serKey);
  if (! brpc_asm(req, CANCEL_FMT_REQ, &_serKey)) {
    ERROR("failed to assemble RPC request: %s [%d].\n", brpc_strerror(),
        brpc_errno);
    brpc_finish(req);
    return NULL;
  }
  return req;
}

#define XTRA_HDRS(_xhdrs, _msg)						\
  string _xhdrs;							\
  if (_msg.route.length())						\
    _xhdrs += _msg.route;						\
  if (_msg.contact.length())						\
    _xhdrs += _msg.contact;						\
  if (_msg.content_type.length())					\
    _xhdrs += SIP_HDR_COLSP(SIP_HDR_CONTENT_TYPE) + _msg.content_type + CRLF;\
  _xhdrs += _msg.hdrs;

static inline brpc_t *build_request(const AmSipRequest &amReq, 
    brpc_int_t as_id)
{
  brpc_t *req;

  if (! (req = brpc_req(SER_REQUEST, random()))) {
    ERROR("failed to build RPC context: %s [%d].\n", brpc_strerror(), 
        brpc_errno);
    return NULL;
  }

  XTRA_HDRS(xtraHdrs, amReq);
  
  STR2BSTR(_method, amReq.method);
  STR2BSTR(_r_uri, amReq.r_uri);
  STR2BSTR(_from, amReq.from);
  STR2BSTR(_to, amReq.to);
  STR2BSTR(_callid, amReq.callid);
  STR2BSTR(_next_hop, amReq.next_hop);
  STR2BSTR(_hdrs, xtraHdrs);
  STR2BSTR(_body, amReq.body);
  STR2BSTR(_empty, string(""));

#define STRIP_HF_NAME(_bstr_, _hf_name, _hf_name_len)  \
  do {  \
    if ((_hf_name_len < (_bstr_)->len) && \
        (strncmp((_bstr_)->val, _hf_name, _hf_name_len) == 0)) { \
      (_bstr_)->val += _hf_name_len; \
      (_bstr_)->len -= _hf_name_len; \
      while (*(_bstr_)->val == ' ') { \
        (_bstr_)->val ++; \
        (_bstr_)->len --; \
      } \
    } \
  } while (0)

  STRIP_HF_NAME(&_from, SIP_HDR_COL(SIP_HDR_FROM), 
      SIP_HDR_LEN(SIP_HDR_COL(SIP_HDR_FROM)));
  STRIP_HF_NAME(&_to, SIP_HDR_COL(SIP_HDR_TO), 
      SIP_HDR_LEN(SIP_HDR_COL(SIP_HDR_TO)));

  if (! brpc_asm(req, REQUEST_FMT_REQ,
      as_id,
      SIP_REQ_FIN_FLG|SIP_REQ_PRV_FLG|SIP_REQ_RUN_ORR, // FIXME: parameterized
      &_method,
      &_r_uri,
      &_from, // FIXME: only HF value; MUST have tag (check)
      &_to, // FIXME: only HF value (no "To: " included) (check)
      amReq.cseq,
      &_callid,
      &_next_hop,
      &_hdrs,
      &_body,
      &_empty // FIXME: "use the power!"
      )) {
    ERROR("failed to assemble RPC request: %s [%d].\n", brpc_strerror(),
        brpc_errno);
    brpc_finish(req);
		return NULL;
  }
  return req;
}

int BrpcCtrlInterface::send(const AmSipRequest &amReq, string &serKey)
{
  int ret = -1;
  brpc_t *req, *rpl = NULL;
  brpc_int_t *code;
  brpc_str_t *ser_opaque;

  if (amReq.method == "CANCEL") {
    req = build_cancel(amReq);
  } else if (amReq.method == "ACK") {
    ERROR("ACK support not yet implemented.\n");
    return -1;
  } else {
    req = build_request(amReq, as_id);
  }

  if (! req)
    return -1;

  rpl = rpcExecute(req);
  req = NULL;
  if (! rpl)
    goto end;

  if (! brpc_dsm(rpl, FMT_RPL, &code, &ser_opaque)) {
    ERROR("failed to disassebmle SER's response: %s [%d].\n", brpc_strerror(), 
        brpc_errno);
    goto end;
  }
  if ((! code) || (! ser_opaque)) {
    ERROR("unexpected NULLs in SER's response (code@%p, opaque@%p).\n",
        code, ser_opaque);
    goto end;
  }
  if (300 <= *code) {
    ERROR("RPC request failed with code: %d, status: '%.*s'.\n", *code,
        /*misleading var. name!*/BRPC_STR_FMT(ser_opaque));
    goto end;
  }
  DBG("SER's opaque/reason: `%.*s'.\n", BRPC_STR_FMT(ser_opaque));
  //len must be fed, as the opaque could contain 0s
  serKey = string(ser_opaque->val, ser_opaque->len);

  ret = 0;
end:
  if (req)
    brpc_finish(req);
  if (rpl)
    brpc_finish(rpl);
  return ret;
}


int BrpcCtrlInterface::send(const AmSipReply &amRpl)
{
  int ret = -1;
  brpc_t *req, *rpl = NULL;
  brpc_int_t *retcode;
  brpc_str_t *ser_opaque;


  if (amRpl.method == "CANCEL") {
    DBG("skipping replying to CANCEL, no longer needed with SER2.\n");
    return 0;
  }

  if (! (req = brpc_req(SER_REPLY, random()))) {
    ERROR("failed to build RPC context: %s [%d].\n", brpc_strerror(), 
        brpc_errno);
    return -1;
  }

  XTRA_HDRS(xtraHdrs, amRpl);

  STR2BSTR(_serKey, amRpl.serKey);
  STR2BSTR(_reason, amRpl.reason);
  STR2BSTR(_local_tag, amRpl.local_tag);
  STR2BSTR(_hdrs, xtraHdrs);
  STR2BSTR(_body, amRpl.body);
  if (! brpc_asm(req, REPLY_FMT_REQ,
      &_serKey,
      amRpl.code,
      &_reason,
      &_local_tag,
      &_hdrs,
      &_body
    )) {
    ERROR("failed to assemble RPC request: %s [%d].\n", brpc_strerror(),
        brpc_errno);
    goto end;
  }
  
  rpl = rpcExecute(req);
  req = NULL;
  if (! rpl)
    goto end;

  if (! brpc_dsm(rpl, FMT_RPL, &retcode, &ser_opaque)) {
    ERROR("failed to disassebmle SER's response: %s [%d].\n", brpc_strerror(), 
        brpc_errno);
    goto end;
  }
  if ((! retcode) || (! ser_opaque)) {
    ERROR("unexpected NULLs in SER's response (code@%p, opaque@%p).\n", 
      retcode, ser_opaque);
    goto end;
  }
  if (300 <= *retcode) {
    ERROR("RPC request failed with code: %d, status: '%.*s'.\n", *retcode,
        /*misleading var. name!*/BRPC_STR_FMT(ser_opaque));
    goto end;
  }

  DBG("successfully posted SER reply event.\n");
  ret = 0;
end:
  if (req)
    brpc_finish(req);
  if (rpl)
    brpc_finish(rpl);
  return ret;
}

string BrpcCtrlInterface::getContact(const string &displayName, 
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
  if (hostName.length()) {
    localUri += SIP_SCHEME_SIP; //TODO: sips|tel|tels
    localUri += ":";
    if (userName.length()) {
      localUri += userName;
      localUri += "@";
    }
    localUri += hostName;
  } else {
    // SER will substituite the markers below
    if (userName.length()) {
      localUri += char(STX);
      localUri += userName;
      localUri += char(ETX);
    } else {
      localUri += char(SUB);
    }
  }

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

  return localUri;
}

