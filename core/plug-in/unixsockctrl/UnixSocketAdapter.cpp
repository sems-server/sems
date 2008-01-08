#include "AmUtils.h"
#include "AmConfig.h"
#include "AmSession.h"
#include "sems.h"
#include "log.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include <assert.h>

#include "UnixSocketAdapter.h"


#define MAX_MSG_ERR   5
#define FIFO_VERSION  "0.3"


#define NREADY(_msg, _args...)  \
  do {  \
    ERROR(_msg, ##_args); \
    return false;  \
  } while (0)

#define CHK_MBR(_member) \
  do {  \
    if (! _member.size())  \
      NREADY("mandatory member empty: '%s'\n", #_member);  \
  } while (0)


int UnixSocketAdapter::getLine(string& line)
{
  int err = get_line(buffer,CTRL_MSGBUF_SIZE);
  if(err != -1)
    line = buffer;
  return err;
}

int UnixSocketAdapter::getLines(string& lines)
{
  int err = get_lines(buffer,CTRL_MSGBUF_SIZE);
  if(err != -1)
    lines = buffer;
  return err;
}

int UnixSocketAdapter::getParam(string& param)
{
  return get_param(param,buffer,CTRL_MSGBUF_SIZE);
}


int UnixSocketAdapter::cacheMsg()
{
  int err_cnt=0;

  msg_c = NULL;
  while(true){

    msg_sz = recv(fd,msg_buf,CTRL_MSGBUF_SIZE,MSG_TRUNC|MSG_DONTWAIT);
    if(msg_sz == -1){
      ERROR("recv on unix socket failed: %s\n",strerror(errno));
      if(++err_cnt >= MAX_MSG_ERR){
        ERROR("too many consecutive errors...\n");
        return -1;
      }

      continue;
    }

    break;
  }

  if(msg_sz > CTRL_MSGBUF_SIZE){
    ERROR("unix socket message is too big (size=%i;max=%i): discarding\n",
	  msg_sz,CTRL_MSGBUF_SIZE);
    return -1;
  }

  msg_buf[msg_sz-1] = '\0';
  msg_c = msg_buf;

  DBG("recv-ed:\n<<%s>>\n",msg_buf);

  return 0;
}

int UnixSocketAdapter::get_line(char* lb, unsigned int lbs)
{
  assert(msg_c);
  return msg_get_line(msg_c,lb,lbs);
}

int UnixSocketAdapter::get_lines(char* lb, unsigned int lbs)
{
  assert(msg_c);
  return msg_get_lines(msg_c,lb,lbs);
}

int UnixSocketAdapter::get_param(string& p, char* lb, unsigned int lbs)
{
  assert(msg_c);
  return msg_get_param(msg_c,p,lb,lbs);
}

UnixSocketAdapter::UnixSocketAdapter()
  : close_fd(true),msg_c(NULL),msg_sz(0), fd(0)
{
  memset(sock_name,0,UNIX_PATH_MAX);
}

UnixSocketAdapter::~UnixSocketAdapter()
{
  close();
}

bool UnixSocketAdapter::init(const string& addr)
{
  strncpy(sock_name,addr.c_str(),UNIX_PATH_MAX-1);

  ::unlink(sock_name);
  fd = create_unix_socket(sock_name);
  if(fd == -1){
    ERROR("could not open unix socket '%s'\n",sock_name);
    return false;
  }

  DBG("UnixSocketAdapter::init @ %s\n", sock_name);
  close_fd = true;
  return true;
}

int UnixSocketAdapter::sendto(const string& addr,const char* buf,unsigned int len)
{
  return write_to_socket(fd,addr.c_str(),buf,len);
}

void UnixSocketAdapter::close()
{
  if((fd != -1) && close_fd){
    ::close(fd);
  }

  fd = -1;

  if(sock_name[0] != '\0')
    ::unlink(sock_name);
}

/**
 * Return:
 *    -1 if error.
 *     0 if timeout.
 *     1 if there some datas ready.
 */
int UnixSocketAdapter::wait4data(int timeout)
{
  struct pollfd pfd = { fd, POLLIN, 0 };

  int ret = poll(&pfd,1,timeout);
  if(ret < 0){
    ERROR("poll: %s\n",strerror(errno));
    return -1;
  }
  else if(ret == 0){
    WARN("poll timed out\n");
    return -1;
  }

  if(pfd.revents & POLLIN)
    return 1;
  else {
    ERROR("poll: revents & POLLIN == 0\n");
    return -1;
  }
}


#define SAFECTRLCALL1(fct, arg1) \
  do {  \
    int ret;  \
    if((ret = fct(arg1)) < 0){ \
       ERROR("call %s(%s) failed with %d\n", #fct, #arg1, ret);  \
       goto failure; \
    } \
  } while (0)


bool UnixSocketAdapter::receive(AmSipRequest &req)
{
  string            version;
  string            fct_name;
  string            cmd;
  string::size_type pos;
  string cseq_str;

  if (cacheMsg() < 0)
    goto failure;

  // handle API version
  SAFECTRLCALL1(getParam, version);
  if (version == "") {
    // some odd trailer from previous request -- ignore
    ERROR("odd trailer\n");
    goto failure;
  }
  if(version != FIFO_VERSION){
    ERROR("wrong FIFO Interface version: %s\n",version.c_str());
    goto failure;
  }

  // handle invoked function
  SAFECTRLCALL1(getParam, fct_name);
  if((pos = fct_name.find('.')) != string::npos){
    cmd = fct_name.substr(pos+1,string::npos);
    fct_name = fct_name.substr(0,pos);
  }
  if(fct_name != "sip_request") {
    ERROR("unexpected request function: '%s'\n",fct_name.c_str());
    goto failure;
  }
  if(cmd.empty()) {
    ERROR("parameter plug-in name missing.\n");
    goto failure;
  }
  req.cmd = cmd;

#define READ_PARAMETER(p) \
  do {  \
    SAFECTRLCALL1(getParam, p); \
    DBG("%s = <%s>\n",#p,p.c_str());  \
  } while (0)

  READ_PARAMETER(req.method);
  READ_PARAMETER(req.user);
  READ_PARAMETER(req.domain);
  READ_PARAMETER(req.dstip);    // will be taken for UDP's local peer IP & Contact
  READ_PARAMETER(req.port);     // will be taken for Contact
  READ_PARAMETER(req.r_uri);    // ??? will be taken for the answer's Contact header field ???
  READ_PARAMETER(req.from_uri); // will be taken for subsequent request uri
  READ_PARAMETER(req.from);
  READ_PARAMETER(req.to);
  READ_PARAMETER(req.callid);
  READ_PARAMETER(req.from_tag);
  READ_PARAMETER(req.to_tag);

  READ_PARAMETER(cseq_str);
    
  if(sscanf(cseq_str.c_str(),"%u", &req.cseq) != 1){
    ERROR("invalid cseq number '%s'\n",cseq_str.c_str());
    goto failure;
  }
  DBG("cseq = <%s>(%i)\n",cseq_str.c_str(),req.cseq);
    
  READ_PARAMETER(req.serKey);
  READ_PARAMETER(req.route);
  READ_PARAMETER(req.next_hop);


  SAFECTRLCALL1(getLines,req.hdrs);
  DBG("hdrs = <%s>\n",req.hdrs.c_str());

  SAFECTRLCALL1(getLines,req.body);
  DBG("body = <%s>\n",req.body.c_str());

  if(req.from.empty() || 
     req.to.empty() || 
     req.callid.empty() || 
     req.from_tag.empty()) {
    ERROR("%s::%s: empty mandatory parameter (from|to|callid|from_tag)\n", 
      __FILE__, __FUNCTION__);
    goto failure;
  }

  return true;

failure:
  return false;

#undef READ_PARAMETER
}


bool UnixSocketAdapter::receive(AmSipReply &reply)
{
  string tmp_str;
  string cseq_str;

#ifdef OpenSER
  unsigned int mi_res_code;
  string mi_res_msg;
#endif

  if (cacheMsg() < 0)
    goto failure;

#ifdef OpenSER
  SAFECTRLCALL1(getParam,tmp_str);
    
  DBG("MI response from OpenSER: %s\n",tmp_str.c_str());
  if( parse_return_code(tmp_str.c_str(),// res_code_str,
      mi_res_code, mi_res_msg) == -1 ){
    ERROR("while parsing MI return code from OpenSER.\n");
    goto failure;
  }

  if (mi_res_code != 200) {
      ERROR("MI response from OpenSER\n");
      goto failure;
  }

  SAFECTRLCALL1(getParam,tmp_str);
  DBG("SIP response from OpenSER: %s\n",tmp_str.c_str());
  if( parse_return_code(tmp_str.c_str(),// res_code_str,
      reply.code, reply.reason) == -1 ){
    ERROR("while parsing return code from Ser.\n");
    goto failure;
  }
#else
  SAFECTRLCALL1(getParam,tmp_str);

  DBG("response from Ser: %s\n",tmp_str.c_str());
  if( parse_return_code(tmp_str.c_str(),// res_code_str,
      reply.code,reply.reason) == -1 ){
    ERROR("while parsing return code from Ser.\n");
    goto failure;
  }
#endif

  /* Parse complete response:
   *
   *   [next_request_uri->cmd.from_uri]CRLF
   *   [next_hop->cmd.next_hop]CRLF
   *   [route->cmd.route]CRLF
   *   ([headers->n_cmd.hdrs]CRLF)*
   *   CRLF
   *   ([body->body]CRLF)*
   */
	
  SAFECTRLCALL1(getParam,reply.next_request_uri);
  SAFECTRLCALL1(getParam,reply.next_hop);
  SAFECTRLCALL1(getParam,reply.route);

  SAFECTRLCALL1(getLines,reply.hdrs);
  SAFECTRLCALL1(getLines,reply.body);

  if(reply.hdrs.empty()){
    ERROR("reply is missing headers: <%i %s>\n",
	  reply.code,reply.reason.c_str());
    goto failure;
  }

  reply.local_tag = getHeader(reply.hdrs,"From");
  reply.local_tag = extract_tag(reply.local_tag);

  reply.remote_tag = getHeader(reply.hdrs,"To");
  reply.remote_tag = extract_tag(reply.remote_tag);

  cseq_str   = getHeader(reply.hdrs,"CSeq");
  if(str2i(cseq_str,reply.cseq)){
    ERROR("could not parse CSeq header\n");
    goto failure;
  }

  return true;
failure:
  //cleanup(ctrl);
  return false;
}


bool UnixSocketAdapter::isComplete(const AmSipReply &rpl)
{
  if (rpl.code < 100 || 699 < rpl.code)
    NREADY("invalid reply code: %d.\n", rpl.code);

  CHK_MBR(rpl.reason);
  CHK_MBR(rpl.serKey);

  if (300 <= rpl.code)
    return true;

  CHK_MBR(rpl.local_tag);

  if (! rpl.body.empty())
    CHK_MBR(rpl.content_type);

  if ((rpl.method!="CANCEL") && (rpl.method!="BYE"))
    CHK_MBR(rpl.contact);

  return true;
}


#ifdef OpenSER
/* Escape " chars of argument and return escaped string */
static inline string escape(string s)
{
    string::size_type pos;

    pos = 0;
    while ((pos = s.find("\"", pos)) != string::npos) {
	s.insert(pos, "\\");
	pos = pos + 2;
    }
    return s;
}

/* Add CR before each LF if not already there */
static inline string lf2crlf(string s)
{
    string::size_type pos;

    pos = 0;
    while ((pos = s.find("\n", pos)) != string::npos) {
	if ((pos > 0) && (s[pos - 1] == 13)) {
	    pos = pos + 1;
	} else {
	    s.insert(pos, "\r");
	    pos = pos + 2;
	}
    }
    return s;
}
#endif

string UnixSocketAdapter::serialize(const AmSipReply &reply, 
    const string &rplAddr)
{
  string msg;

  msg = ":t_reply:" 
#ifndef OpenSER
    + rplAddr +
#endif
    "\n";

  msg += int2str(reply.code);
  msg += "\n";

  msg += reply.reason;
  msg += "\n";

  msg += reply.serKey;
  msg += "\n";

  msg += reply.local_tag;
  msg += "\n";

  string extraHdrs, bodyFrame;

  if (AmConfig::Signature.length())
    extraHdrs += "Server: " + AmConfig::Signature + "\n";

  if (! reply.hdrs.empty())
    extraHdrs += reply.hdrs;

  if (reply.code < 300) {
    if (! reply.contact.empty())
      extraHdrs += reply.contact;

    if (! reply.body.empty())
      extraHdrs += "Content-Type: " + reply.content_type + "\n";

#ifndef OpenSER
    bodyFrame += reply.body;
    bodyFrame += ".\n\n";
#else
    if (! reply.body.empty()) {
      // TODO: body already CRLF'ed?
      bodyFrame += "\"" + reply.body + "\"\n";
    }
#endif
  } 

#ifndef OpenSER
  else {
    bodyFrame = ".\n\n";
  }
  extraHdrs += ".\n";
#endif

#ifdef OpenSER
  if (extraHdrs.empty()) {
      extraHdrs = ".\n";
  } else {
      extraHdrs = "\"" + lf2crlf(escape(extraHdrs)) + "\"\n";
  }
#endif

  msg += extraHdrs + bodyFrame;
  return msg;
}

int UnixSocketAdapter::send_msg(const string &msg, const string &src, 
    const string &dst, int timeout)
{
  DBG("sending out serialized SER command:\n<<%s>>.\n", msg.c_str());

  // FIXME: is this init really needed?!
  //if(init(src) || sendto(dst, msg.c_str(),msg.length())){
  if(sendto(dst, msg.c_str(),msg.length())){
    ERROR("...while sending request to SER.\n");
    return -1;
  }

  if (timeout) { // should it collect the reply?
    if(wait4data(timeout) < 1){ 
      ERROR("while waiting for SER's response\n");
      return -1;
    }

    string status_line;
    if(cacheMsg() || getParam(status_line)) 
      return -1;

    unsigned int res_code;
    string res_reason;
    if(parse_return_code(status_line.c_str(),res_code,res_reason))
      return -1;

    if( (res_code < 200) ||
        (res_code >= 300) ) {
      ERROR("SER answered: %i %s\n", res_code,res_reason.c_str());
      return -1;
    }
  }

  return 0;

}

int UnixSocketAdapter::send(const AmSipReply &reply, const string &dst)
{
  string rplSockAddr, msg;

  if (! isComplete(reply)) {
    ERROR("can not send reply: not complete.\n");
    return -1;
  }

  rplSockAddr = /*TODO: configurable?!*/"/tmp/" + AmSession::getNewId();
  msg = serialize(reply, rplSockAddr);
  return send_msg(msg, rplSockAddr, dst, /*TODO: configurable?!*/500);
}



bool UnixSocketAdapter::isComplete(const AmSipRequest &req)
{
  CHK_MBR(req.method);
  CHK_MBR(req.callid);
  if (req.method != "CANCEL") {
    CHK_MBR(req.r_uri);
    CHK_MBR(req.from);
    CHK_MBR(req.to);
    if (! req.body.empty())
      CHK_MBR(req.content_type);
  }

  return true;
}

string UnixSocketAdapter::serialize(const AmSipRequest& req, 
    const string &rplAddr)
{
  string msg;

  msg = ":t_uac_dlg:" 
#ifndef OpenSER
    + rplAddr + 
#endif
    "\n"
    + req.method + "\n"
    + req.r_uri + "\n";
    
  if(req.next_hop.empty())
    msg += ".";
  else
    msg += req.next_hop;

  msg += "\n";

#ifdef OpenSER
  msg += ".\n"; /* socket */
#endif

  string extraHdrs;

  extraHdrs += req.from;
  extraHdrs += "\n";
    
  extraHdrs += req.to;
  extraHdrs += "\n";
    
  extraHdrs += "CSeq: " + int2str(req.cseq) + " " + req.method + "\n"
    + "Call-ID: " + req.callid + "\n";
   
  if (! req.contact.empty())
    extraHdrs += req.contact;
    
  if (! req.hdrs.empty()) {
    extraHdrs += req.hdrs;
    if (req.hdrs[req.hdrs.length() - 1] != '\n')
      extraHdrs += "\n";
  }

  if(!req.route.empty())
    extraHdrs += req.route;
    
  if(!req.body.empty())
    extraHdrs += "Content-Type: " + req.content_type + "\n";
    
  extraHdrs += "Max-Forwards: " /*TODO: configurable?!*/MAX_FORWARDS "\n";

  if (AmConfig::Signature.length()) 
    extraHdrs += "User-Agent: " + AmConfig::Signature + "\n";

#ifdef OpenSER
  extraHdrs = "\"" + lf2crlf(escape(extraHdrs)) + "\"\n";
#endif

  string bodyFrame;
#ifndef OpenSER
  bodyFrame = ".\n" // EoH
    + req.body + ".\n\n";
#else
  // is lf2crlf() needed?! (see function for replies)
  if (!req.body.empty())
      bodyFrame = "\"" + lf2crlf(req.body) + "\"\n";
#endif

  msg += extraHdrs + bodyFrame;

  return msg;
}

string UnixSocketAdapter::serialize_cancel(const AmSipRequest& req, 
    const string &rplAddr)
{
  string msg;

  msg = ":t_uac_cancel:" 
#ifndef OpenSER
    + rplAddr + 
#endif
    "\n" +
    req.callid + "\n" +
    int2str(req.cseq) + "\n"
#ifndef OpenSER
    "\n"
#endif
    ;
  return msg;
}

int UnixSocketAdapter::send(const AmSipRequest &req, string &src, string &dst)
{
  if (! isComplete(req)) {
    ERROR("can not send request: not complete.\n");
    return -1;
  }

  string rplAddr;
  string msg;
  int timeout;
  if (req.method == "CANCEL") {
    rplAddr = "/tmp/" + AmSession::getNewId();
    msg = serialize_cancel(req, rplAddr);
    timeout = 50000; /*TODO: WTF's with this value?!*/
  } else {
    rplAddr = src;//AmConfig::ReplySocketName;
    msg = serialize(req, rplAddr);
    /* timeout: don't wait for reply [comes through other, 
     * dedicated socket] */
    timeout = 0;
  }

  return send_msg(msg, rplAddr, dst, timeout);
}
